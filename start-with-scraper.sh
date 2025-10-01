#!/bin/bash
# Quick start script for Trading212 scraper + backend

set -e

echo "=== Trading212 Scraper + Backend Setup ==="
echo ""

# Check if scraper dependencies are installed
if [ ! -d "scraper/node_modules" ]; then
    echo "Installing scraper dependencies..."
    cd scraper
    npm install
    cd ..
    echo "✓ Dependencies installed"
fi

# Start scraper in background
echo ""
echo "Starting scraper service on http://localhost:9000..."
cd scraper
npm start &
SCRAPER_PID=$!
cd ..

# Wait for scraper to be ready
echo "Waiting for scraper to be ready..."
for i in {1..10}; do
    if curl -s http://localhost:9000/health > /dev/null 2>&1; then
        echo "✓ Scraper is ready"
        break
    fi
    if [ $i -eq 10 ]; then
        echo "✗ Scraper failed to start"
        kill $SCRAPER_PID 2>/dev/null || true
        exit 1
    fi
    sleep 1
done

# Build backend if needed
if [ ! -f "exchange-backend/exchange-backend" ] || [ "exchange-backend/http_server.cpp" -nt "exchange-backend/exchange-backend" ]; then
    echo ""
    echo "Building backend..."
    cd exchange-backend
    make -j4
    cd ..
    echo "✓ Backend built"
fi

# Start backend
echo ""
echo "Starting backend with TRADING212 provider..."
echo "Press Ctrl+C to stop both services"
echo ""

cd exchange-backend
STOCKS_PROVIDER=TRADING212 \
STOCKS_SYMBOLS=AAPL,MSFT,TSLA \
STOCKS_REFRESH_SECONDS=60 \
SCRAPER_URL=http://localhost:9000 \
./exchange-backend

# Cleanup on exit
trap "echo 'Stopping services...'; kill $SCRAPER_PID 2>/dev/null || true" EXIT
