# Trading212 Scraper - Known Issues

## Current Status: NOT WORKING

The Trading212 scraper implementation is encountering several technical and practical issues:

### 1. Puppeteer/macOS Permission Issues ❌

**Error**: `socket hang up` when launching Puppeteer
**Cause**: macOS Gatekeeper blocks the Chrome for Testing binary downloaded by Puppeteer
**Attempted fixes**:

- Installed Chrome via `npx puppeteer browsers install chrome`
- Tried removing quarantine attributes (`xattr -d com.apple.quarantine`)
- Tried `--no-sandbox` flag

**Impact**: Cannot launch headless browser on macOS

### 2. Trading212 Anti-Scraping Measures 🛡️

Even if Puppeteer works, Trading212 likely implements:

- Bot detection (Cloudflare, DataDome, etc.)
- Rate limiting
- IP blocking
- CAPTCHA challenges

### 3. Maintenance Burden 🔧

- CSS selectors need constant updates when Trading212 changes their UI
- Current selectors in `scraper/server.js` are **placeholders only**
- Requires frequent manual inspection and updates

### 4. Legal/ToS Concerns ⚖️

- Automated scraping may violate Trading212's Terms of Service
- Could result in account suspension or IP bans
- "Permission" claim should be verified in writing

## Recommended Alternative: Use STOOQ Provider

The STOOQ provider is:

- ✅ **Working reliably** (free CSV API)
- ✅ **No authentication required**
- ✅ **Officially supported** data source
- ✅ **Stable and maintained**

### Quick Switch Back to STOOQ

```bash
# In terminal:
cd /Users/leonmamic/Documents/coding/github/sax-trading/exchange-backend
make -j4

# Start backend with STOOQ:
STOCKS_PROVIDER=STOOQ \
STOCKS_SYMBOLS=AAPL,MSFT,TSLA,GOOGL,AMZN \
STOCKS_REFRESH_SECONDS=60 \
./exchange-backend
```

Or use the absolute path if permission issues persist:

```bash
/Users/leonmamic/Documents/coding/github/sax-trading/exchange-backend/exchange-backend
```

### Start Frontend

```bash
cd /Users/leonmamic/Documents/coding/github/sax-trading/exchange-frontend
npm start
```

Visit http://localhost:3000

## If You Still Want to Try the Scraper

### Option 1: Try on Linux

Puppeteer works more reliably on Linux without permission issues.

### Option 2: Use a Different Scraping Approach

- Use Trading212's mobile app API (if available)
- Request official API access from Trading212
- Use a scraping service like ScraperAPI or Bright Data (paid)

### Option 3: Fix macOS Puppeteer

Try running with sudo (not recommended for security):

```bash
sudo node scraper/server.js
```

Or manually approve the Chrome binary:

1. Open Finder
2. Navigate to: `~/.cache/puppeteer/chrome/mac_arm-121.0.6167.85/chrome-mac-arm64/`
3. Right-click "Google Chrome for Testing.app"
4. Click "Open"
5. Click "Open" in security dialog
6. Try running scraper again

## Summary

**Current recommendation**: Use STOOQ provider (default) as it's working, free, and reliable. The Trading212 scraper is blocked by macOS security and would face additional challenges even if that was resolved.

The backend supports multiple providers via `STOCKS_PROVIDER` env var:

- `STOOQ` - Free CSV API (recommended) ✅
- `YAHOO` - Requires authentication (429 errors) ❌
- `TRADING212` - Scraper-based (blocked on macOS) ❌
