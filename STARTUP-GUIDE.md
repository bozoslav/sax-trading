# Complete Startup Guide

## Quick Start (Recommended - STOOQ Provider)

### 1. Start the Backend

```bash
cd /Users/leonmamic/Documents/coding/github/sax-trading/exchange-backend

# Build if needed:
make -j4

# Start with STOOQ provider (use absolute path to avoid macOS permission issues):
STOCKS_PROVIDER=STOOQ \
STOCKS_SYMBOLS=AAPL,MSFT,TSLA,GOOGL,AMZN,NVDA,META \
STOCKS_REFRESH_SECONDS=60 \
/Users/leonmamic/Documents/coding/github/sax-trading/exchange-backend/exchange-backend
```

**Expected output:**

```
Starting server on port 8080
[stooq] fetching symbols=AAPL.US,MSFT.US,TSLA.US...
HTTP server listening on port 8080 (stocks refresh=60s)
[stooq] parsed_rows=X
```

### 2. Test the Backend

In a new terminal:

```bash
curl http://localhost:8080/stocks
```

**Expected response:**

```json
[
  {
    "symbol": "MSFT",
    "name": "MSFT.US",
    "price": 513.45,
    "change": -1.35,
    "percent": -0.26
  },
  {
    "symbol": "TSLA",
    "name": "TSLA.US",
    "price": 454.98,
    "change": 11.18,
    "percent": 2.52
  }
]
```

With headers showing provider and data age:

```
X-Data-Provider: STOOQ
X-Data-Age-Seconds: 5
X-Data-Refresh-Seconds: 60
X-Data-Symbols: AAPL,MSFT,TSLA
```

### 3. Start the Frontend

In a new terminal:

```bash
cd /Users/leonmamic/Documents/coding/github/sax-trading/exchange-frontend

# Install dependencies if not done yet:
npm install

# Start the React app:
npm start
```

The app will automatically open at http://localhost:3000

### 4. Verify Everything Works

- Frontend should display a table with stocks
- You should see a **STOOQ** chip showing the provider
- Prices should update automatically every minute (or your configured refresh interval)
- If data becomes stale (age > 2× refresh interval), a **STALE** badge appears

---

## Environment Variables

### Backend (exchange-backend)

| Variable                 | Default                 | Description                                           |
| ------------------------ | ----------------------- | ----------------------------------------------------- |
| `PORT`                   | `8080`                  | HTTP server port                                      |
| `STOCKS_PROVIDER`        | `STOOQ`                 | Provider: `STOOQ`, `YAHOO`, or `TRADING212`           |
| `STOCKS_SYMBOLS`         | `AAPL`                  | Comma-separated list of symbols                       |
| `STOCKS_REFRESH_SECONDS` | `60`                    | How often to refresh stock data                       |
| `SCRAPER_URL`            | `http://localhost:9000` | Trading212 scraper URL (if using TRADING212 provider) |

### Frontend (exchange-frontend)

React app uses default CRA settings. Backend URL is hardcoded as `http://localhost:8080`.

---

## Troubleshooting

### "Permission denied" on macOS when running `./exchange-backend`

**Solution**: Use the absolute path instead:

```bash
/Users/leonmamic/Documents/coding/github/sax-trading/exchange-backend/exchange-backend
```

Or move to another directory first:

```bash
cd /Users/leonmamic/Documents/coding/github/sax-trading
./exchange-backend/exchange-backend
```

### Backend compiles but doesn't show any stocks

**Possible causes:**

1. Stooq returned incomplete data (some symbols may be filtered)
2. All symbols had zero prices (filtered out)
3. Network issues reaching Stooq

**Debug**: Check the backend logs for `[stooq] parsed_rows=X`. If `X=0`, try different symbols:

```bash
STOCKS_SYMBOLS=MSFT,TSLA,GOOGL,AMZN
```

### Frontend shows "Failed to fetch stocks"

**Possible causes:**

1. Backend not running
2. Backend running on different port
3. CORS issue

**Solution**:

1. Verify backend is running: `curl http://localhost:8080/stocks`
2. Check backend logs for errors
3. Restart both backend and frontend

### "No stocks found" in frontend

This means the backend is running but returning an empty array. Check:

1. Backend logs show successful data fetch
2. Symbols are valid (use US symbols like `AAPL`, not `AAPL.L`)
3. Try different symbols

### Data shows as "STALE"

This means the backend hasn't refreshed data in more than 2× the refresh interval. Possible causes:

1. Network issues
2. Provider (Stooq) is down or rate limiting
3. Backend refresh thread encountered an error

**Solution**: Restart the backend or check logs for errors.

---

## Provider Comparison

| Provider       | Status         | Pros                      | Cons                                         |
| -------------- | -------------- | ------------------------- | -------------------------------------------- |
| **STOOQ**      | ✅ Working     | Free, no auth, reliable   | EOD data, may filter symbols                 |
| **YAHOO**      | ❌ Broken      | Real-time data            | Requires auth, rate limited (401/429 errors) |
| **TRADING212** | ❌ Not Working | Matches Trading212 prices | Scraper blocked on macOS, legal concerns     |

**Recommendation**: Use **STOOQ** (default).

---

## Complete Example Session

```bash
# Terminal 1: Backend
cd ~/Documents/coding/github/sax-trading/exchange-backend
make -j4
STOCKS_PROVIDER=STOOQ STOCKS_SYMBOLS=MSFT,TSLA,NVDA,AMZN STOCKS_REFRESH_SECONDS=60 \
  /Users/leonmamic/Documents/coding/github/sax-trading/exchange-backend/exchange-backend

# Terminal 2: Test
curl http://localhost:8080/stocks

# Terminal 3: Frontend
cd ~/Documents/coding/github/sax-trading/exchange-frontend
npm install
npm start

# Browser: http://localhost:3000
```

---

## Stopping the Services

```bash
# Kill backend
killall exchange-backend

# Frontend: Just Ctrl+C in the terminal where it's running
```

---

## Next Steps / Future Enhancements

- [ ] Add more reliable provider (e.g., Alpha Vantage with free tier)
- [ ] Persist last snapshot to disk for faster startup
- [ ] Add WebSocket support for real-time updates
- [ ] Implement proper shutdown (join background thread)
- [ ] Add more error handling and retry logic
- [ ] Database integration for historical data
