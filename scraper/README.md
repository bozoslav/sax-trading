# Trading212 Scraper Service

Local HTTP service that scrapes stock prices from Trading212 and caches them.

## Setup

```bash
cd scraper
npm install
```

## Run

```bash
npm start
```

Server runs on `http://localhost:9000` by default (configure with `SCRAPER_PORT` env var).

## Usage

### Get quotes

```bash
curl "http://localhost:9000/quotes?symbols=AAPL,MSFT,TSLA"
```

Returns JSON array:

```json
[
  {
    "symbol": "AAPL",
    "name": "AAPL",
    "price": 178.25,
    "change": 1.50,
    "percent": 0.85,
    "timestamp": 1696176000000
  },
  ...
]
```

### Health check

```bash
curl http://localhost:9000/health
```

## Features

- **Caching**: 30s TTL per symbol to minimize scraping frequency
- **Rate limiting**: Minimum 5s between scrapes
- **Stale fallback**: Returns cached data if scrape fails
- **Sequential scraping**: One symbol at a time to be polite

## Configuration

Environment variables:

- `SCRAPER_PORT`: Server port (default 9000)

## Notes

⚠️ **Selector maintenance**: The CSS selectors in `server.js` are placeholders. You must inspect the actual Trading212 page DOM and update the selectors in the `page.evaluate()` section to match their current structure.

To find the correct selectors:

1. Open https://www.trading212.com/trading-instruments/AAPL in a browser
2. Right-click the price → Inspect
3. Note the class names or data attributes
4. Update the `priceSelectors` and `changeSelectors` arrays in `server.js`

## Integration with Backend

The C++ backend calls this service via HTTP:

```cpp
// In http_server.cpp with provider=TRADING212
// GET http://localhost:9000/quotes?symbols=AAPL,MSFT,TSLA
```
