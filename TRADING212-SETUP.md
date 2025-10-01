# Trading212 Scraper Integration

Complete setup for scraping Trading212 stock prices and serving them through your backend.

## Architecture

```
Frontend (React)
    ↓ HTTP
Backend (C++)
    ↓ HTTP localhost
Scraper Service (Node/Puppeteer)
    ↓ Web scraping
Trading212 Website
```

## Quick Start

### Option 1: Automated Script

```bash
./start-with-scraper.sh
```

This will:

1. Install scraper dependencies (npm install)
2. Start the scraper service on port 9000
3. Build the backend
4. Start the backend with TRADING212 provider

### Option 2: Manual Steps

**Terminal 1 - Start Scraper:**

```bash
cd scraper
npm install
npm start
```

**Terminal 2 - Build & Run Backend:**

```bash
cd exchange-backend
make -j4
STOCKS_PROVIDER=TRADING212 \
STOCKS_SYMBOLS=AAPL,MSFT,TSLA \
STOCKS_REFRESH_SECONDS=60 \
./exchange-backend
```

**Terminal 3 - Run Frontend:**

```bash
cd sax-trading  # (the inner one with package.json)
npm start
```

## Configuration

### Environment Variables

**Backend:**

- `STOCKS_PROVIDER=TRADING212` - Use Trading212 scraper
- `STOCKS_SYMBOLS=AAPL,MSFT,TSLA` - Symbols to fetch
- `STOCKS_REFRESH_SECONDS=60` - Background refresh interval
- `SCRAPER_URL=http://localhost:9000` - Scraper service URL (optional, default shown)
- `PORT=8080` - Backend port (optional)

**Scraper:**

- `SCRAPER_PORT=9000` - Scraper service port (optional)

## Testing

### 1. Test Scraper Directly

```bash
# Health check
curl http://localhost:9000/health

# Get quotes
curl "http://localhost:9000/quotes?symbols=AAPL,MSFT"
```

### 2. Test Backend

```bash
curl -i http://localhost:8080/stocks
```

Should see:

- `X-Data-Provider: TRADING212`
- JSON array with scraped prices

### 3. Open Frontend

Navigate to http://localhost:3000

You should see:

- Provider chip showing "TRADING212"
- Live prices from Trading212
- Auto-refresh every 60s (or your configured interval)

## Important: Update Selectors

⚠️ **Before using, you MUST update the CSS selectors** in `scraper/server.js` to match Trading212's actual page structure.

1. Open a Trading212 instrument page: https://www.trading212.com/trading-instruments/AAPL
2. Right-click on the price → Inspect Element
3. Note the class names, data attributes, or IDs
4. Edit `scraper/server.js` and update the selectors in the `page.evaluate()` section:

```javascript
const priceSelectors = [
  ".actual-selector-from-page", // ← Update these
  '[data-qa="price"]',
  // ... add more as needed
];
```

## How It Works

1. **Scraper Service** (Port 9000):

   - Receives requests for stock symbols
   - Uses Puppeteer (headless Chrome) to load Trading212 pages
   - Extracts price, change, and percent from the DOM
   - Caches results for 30s per symbol
   - Rate-limits to minimum 5s between scrapes
   - Returns JSON to the backend

2. **Backend** (Port 8080):

   - Background thread calls scraper every 60s (configurable)
   - Caches scraped data in memory
   - Serves all frontend requests from cache (no per-request scraping)
   - Adds headers (provider, age, staleness)

3. **Frontend** (Port 3000):
   - Polls backend every refresh interval
   - Displays provider chip and staleness badge
   - Shows last updated time

## Advantages

✓ **No registration** - Direct scraping with permission  
✓ **Server-side only** - Only your backend scrapes, all users share the cache  
✓ **Rate-limited** - Respects Trading212 with delays and caching  
✓ **Real prices** - Same data you see in Trading212 UI  
✓ **Fallback-ready** - Can switch providers via env var

## Troubleshooting

**"Failed to scrape any symbols"**

- Check that Trading212 is accessible
- Update CSS selectors in `scraper/server.js`
- Check scraper logs for specific errors

**"scraper_upstream=500"**

- Scraper service is down or returned an error
- Check `http://localhost:9000/health`
- Restart scraper: `cd scraper && npm start`

**Zero or missing prices**

- Selectors may be wrong - inspect page and update them
- Symbol may not be available on Trading212
- Check scraper logs for extraction details

**Backend shows STOOQ instead of TRADING212**

- Ensure `STOCKS_PROVIDER=TRADING212` is set when running backend
- Check backend startup logs for provider confirmation

## Selector Update Example

After inspecting Trading212:

```javascript
// If you find the price is in a span with class "ticker-price-value"
const priceSelectors = [
  ".ticker-price-value",
  // keep fallbacks...
];

// If change is in a div with data-testid="price-change"
const changeSelectors = [
  '[data-testid="price-change"]',
  // keep fallbacks...
];
```

## Next Steps

Once working:

- Add more symbols to `STOCKS_SYMBOLS`
- Adjust `STOCKS_REFRESH_SECONDS` (lower = fresher, higher = fewer scrapes)
- Monitor scraper logs to ensure no errors
- Consider adding error notifications in the frontend

## Files Created

- `scraper/package.json` - Node dependencies
- `scraper/server.js` - Scraper HTTP service
- `scraper/README.md` - Scraper docs
- `start-with-scraper.sh` - Quick start script
- Backend updates in `exchange-backend/http_server.cpp`

## Support

If you need help updating selectors or troubleshooting, check the scraper logs and Trading212's page structure. The selectors are the only manual configuration needed.
