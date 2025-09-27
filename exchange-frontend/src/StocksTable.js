import React, { useEffect, useState, useRef, useCallback } from 'react';
import { Paper, Table, TableBody, TableCell, TableContainer, TableHead, TableRow, Typography, Box, LinearProgress, IconButton, Tooltip, Chip } from '@mui/material';
import RefreshIcon from '@mui/icons-material/Refresh';

const columns = [
  { id: 'symbol', label: 'Symbol' },
  { id: 'name', label: 'Name' },
  { id: 'price', label: 'Price' },
  { id: 'change', label: 'Change' },
  { id: 'percent', label: 'Change (%)' },
];

export default function StocksTable() {
  const [stocks, setStocks] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [refreshing, setRefreshing] = useState(false);
  const [lastUpdated, setLastUpdated] = useState(null);
  const [stale, setStale] = useState(false);
  const [provider, setProvider] = useState(null);
  const [refreshInterval, setRefreshInterval] = useState(15000); // ms
  const [backendRefreshSeconds, setBackendRefreshSeconds] = useState(null);
  const timerRef = useRef(null);
  const countdownRef = useRef(null);
  const [nextUpdateIn, setNextUpdateIn] = useState(null);

  const fetchStocks = useCallback(async (showSpinner = false) => {
    try {
      if (showSpinner) setRefreshing(true);
      const res = await fetch('http://localhost:8080/stocks');
      const text = await res.text();
      if (!res.ok) {
        try {
          const errJson = JSON.parse(text);
          throw new Error(errJson.message || 'Failed to fetch stocks');
        } catch {
          throw new Error(text || 'Failed to fetch stocks');
        }
      }
      const data = JSON.parse(text);
      setStocks(data);
      setError(null);
      setLastUpdated(new Date());
      // headers
      const age = res.headers.get('X-Data-Age-Seconds');
      const staleHeader = res.headers.get('X-Data-Stale');
      const providerHeader = res.headers.get('X-Data-Provider');
      const refreshSecondsHeader = res.headers.get('X-Data-Refresh-Seconds');
      setStale(!!staleHeader);
      if (providerHeader) setProvider(providerHeader);
      if (refreshSecondsHeader && !isNaN(Number(refreshSecondsHeader))) {
        setBackendRefreshSeconds(Number(refreshSecondsHeader));
      }
      setNextUpdateIn(backendRefreshSeconds ? backendRefreshSeconds : null);
    } catch (e) {
      setError(e.message);
    } finally {
      if (loading) setLoading(false);
      setRefreshing(false);
    }
  }, [loading, backendRefreshSeconds]);

  // Adjust polling interval to backend if available
  useEffect(() => {
    if (backendRefreshSeconds) {
      const newMs = Math.max(backendRefreshSeconds * 1000, 5000); // lower bound 5s
      if (newMs !== refreshInterval) {
        setRefreshInterval(newMs);
        if (timerRef.current) clearInterval(timerRef.current);
        timerRef.current = setInterval(() => fetchStocks(), newMs);
      }
    }
  }, [backendRefreshSeconds, fetchStocks, refreshInterval]);

  useEffect(() => {
    fetchStocks();
    if (!timerRef.current) {
      timerRef.current = setInterval(() => fetchStocks(), refreshInterval);
    }
    return () => {
      if (timerRef.current) clearInterval(timerRef.current);
      if (countdownRef.current) clearInterval(countdownRef.current);
    };
  }, [fetchStocks, refreshInterval]);

  // Countdown timer for next update (frontend poll) when backend interval known
  useEffect(() => {
    if (!backendRefreshSeconds) return;
    if (countdownRef.current) clearInterval(countdownRef.current);
    setNextUpdateIn(backendRefreshSeconds);
    countdownRef.current = setInterval(() => {
      setNextUpdateIn(prev => {
        if (prev === null) return null;
        if (prev <= 1) return backendRefreshSeconds; // reset after polling fires
        return prev - 1;
      });
    }, 1000);
    return () => { if (countdownRef.current) clearInterval(countdownRef.current); };
  }, [backendRefreshSeconds]);

  const formatNumber = (v) => {
    if (v === null || v === undefined || isNaN(v)) return '-';
    return Number(v).toLocaleString(undefined, { minimumFractionDigits: 2, maximumFractionDigits: 2 });
  };

  return (
    <Box sx={{ mt: 6 }}>
      <Box sx={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', mb: 2, flexWrap: 'wrap', gap: 2 }}>
        <Box sx={{ display: 'flex', alignItems: 'center', gap: 1 }}>
          <Typography variant="h4" gutterBottom sx={{ color: '#fff', fontWeight: 'bold', m: 0 }}>
            Live Stock Prices
          </Typography>
          {provider && (
            <Chip size="small" label={provider} sx={{ bgcolor: '#2d3748', color: '#bbb' }} />
          )}
          {stale && (
            <Chip size="small" color="warning" label="STALE" sx={{ fontWeight: 'bold' }} />
          )}
        </Box>
        <Box sx={{ display: 'flex', alignItems: 'center', gap: 2 }}>
          { lastUpdated && !error && (
            <Typography variant="caption" sx={{ color: stale ? '#FFA726' : '#888' }}>
              Updated {lastUpdated.toLocaleTimeString()} {stale && '(stale)'}
            </Typography>
          ) }
          { backendRefreshSeconds && (
            <Typography variant="caption" sx={{ color: '#666' }}>
              Next poll in {nextUpdateIn ?? '-'}s
            </Typography>
          ) }
          <Tooltip title="Refresh now">
            <span>
              <IconButton size="small" onClick={() => fetchStocks(true)} disabled={refreshing} sx={{ color: '#fff' }}>
                <RefreshIcon fontSize="small" />
              </IconButton>
            </span>
          </Tooltip>
        </Box>
      </Box>
      <Paper sx={{ bgcolor: '#222531', position: 'relative' }}>
        {(loading || refreshing) && (
          <Box sx={{ position: 'absolute', top: 0, left: 0, right: 0 }}>
            <LinearProgress color="primary" />
          </Box>
        )}
        <TableContainer>
          <Table size="small">
            <TableHead>
              <TableRow>
                {columns.map(col => (
                  <TableCell key={col.id} sx={{ color: '#aaa', fontWeight: 'bold' }}>{col.label}</TableCell>
                ))}
              </TableRow>
            </TableHead>
            <TableBody>
              {loading ? (
                <TableRow>
                  <TableCell colSpan={columns.length} align="center" sx={{ color: '#fff' }}>
                    Loading...
                  </TableCell>
                </TableRow>
              ) : error ? (
                <TableRow>
                  <TableCell colSpan={columns.length} align="center" sx={{ color: 'red' }}>
                    {error}
                  </TableCell>
                </TableRow>
              ) : stocks.length === 0 ? (
                <TableRow>
                  <TableCell colSpan={columns.length} align="center" sx={{ color: '#fff' }}>
                    No stocks found.
                  </TableCell>
                </TableRow>
              ) : (
                stocks.map((stock, idx) => {
                  const changeColor = stock.change > 0 ? '#0ECB81' : stock.change < 0 ? '#F6465D' : '#fff';
                  const percentColor = stock.percent > 0 ? '#0ECB81' : stock.percent < 0 ? '#F6465D' : '#fff';
                  return (
                    <TableRow key={idx} hover>
                      <TableCell sx={{ color: '#fff', fontWeight: 'bold' }}>{stock.symbol}</TableCell>
                      <TableCell sx={{ color: '#fff' }}>{stock.name}</TableCell>
                      <TableCell sx={{ color: '#fff' }}>{formatNumber(stock.price)}</TableCell>
                      <TableCell sx={{ color: changeColor }}>{formatNumber(stock.change)}</TableCell>
                      <TableCell sx={{ color: percentColor }}>{formatNumber(stock.percent)}%</TableCell>
                    </TableRow>
                  );
                })
              )}
            </TableBody>
          </Table>
        </TableContainer>
      </Paper>
    </Box>
  );
}
