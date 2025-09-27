import React, { useEffect, useState } from 'react';
import { AppBar, Toolbar, Typography, Container, Paper, Table, TableBody, TableCell, TableContainer, TableHead, TableRow, Box, Divider } from '@mui/material';
import StocksTable from './StocksTable';

const columns = [
	{ id: 'side', label: 'Side' },
	{ id: 'price', label: 'Price' },
	{ id: 'amount', label: 'Amount' },
	{ id: 'status', label: 'Status' },
	{ id: 'created_at', label: 'Time' },
];

export default function HomePage() {
	const [orders, setOrders] = useState([]);
	const [loading, setLoading] = useState(true);
	const [error, setError] = useState(null);

	useEffect(() => {
		fetch('http://localhost:8080/orderbook')
			.then(res => {
				if (!res.ok) throw new Error('Failed to fetch orderbook');
				return res.json();
			})
			.then(data => {
				setOrders(data);
				setLoading(false);
			})
			.catch(err => {
				setError(err.message);
				setLoading(false);
			});
	}, []);

	return (
		<Box sx={{ bgcolor: '#181A20', minHeight: '100vh' }}>
			<AppBar position="static" sx={{ bgcolor: '#1E2329' }}>
				<Toolbar>
					<Typography variant="h6" sx={{ flexGrow: 1, fontWeight: 'bold', letterSpacing: 2 }}>
						SAX Exchange
					</Typography>
				</Toolbar>
			</AppBar>
			<Container maxWidth="md" sx={{ mt: 6 }}>
				<Typography variant="h4" align="center" gutterBottom sx={{ color: '#fff', fontWeight: 'bold' }}>
					Orderbook
				</Typography>
				<Paper sx={{ bgcolor: '#222531' }}>
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
								) : orders.length === 0 ? (
									<TableRow>
										<TableCell colSpan={columns.length} align="center" sx={{ color: '#fff' }}>
											No orders found.
										</TableCell>
									</TableRow>
								) : (
									orders.map((order, idx) => (
										<TableRow key={idx}>
											{columns.map(col => (
												<TableCell key={col.id} sx={{ color: order.side === 'buy' ? '#0ECB81' : order.side === 'sell' ? '#F6465D' : '#fff' }}>
													{order[col.id]}
												</TableCell>
											))}
										</TableRow>
									))
								)}
							</TableBody>
						</Table>
					</TableContainer>
				</Paper>

				<Divider sx={{ my: 6, bgcolor: '#333' }} />

				<StocksTable />
			</Container>
		</Box>
	);
}
