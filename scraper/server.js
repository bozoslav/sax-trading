const express = require('express');
const puppeteer = require('puppeteer');

const app = express();
const PORT = process.env.SCRAPER_PORT || 9000;
const CACHE_TTL_MS = 30000; // 30s cache per symbol to avoid excessive scraping

// In-memory cache: { symbol: { price, change, percent, timestamp } }
const cache = {};

// Rate limiting: track last scrape time globally
let lastScrapeTime = 0;
const MIN_SCRAPE_INTERVAL_MS = 5000; // min 5s between scrapes

async function scrapeSymbol(symbol) {
  const now = Date.now();
  
  // Check cache first
  if (cache[symbol] && (now - cache[symbol].timestamp) < CACHE_TTL_MS) {
    console.log(`[scraper] cache hit for ${symbol}`);
    return cache[symbol];
  }

  // Rate limit globally
  const timeSinceLastScrape = now - lastScrapeTime;
  if (timeSinceLastScrape < MIN_SCRAPE_INTERVAL_MS) {
    const waitMs = MIN_SCRAPE_INTERVAL_MS - timeSinceLastScrape;
    console.log(`[scraper] rate limit: waiting ${waitMs}ms before scraping ${symbol}`);
    await new Promise(resolve => setTimeout(resolve, waitMs));
  }

  console.log(`[scraper] fetching ${symbol} from Trading212...`);
  lastScrapeTime = Date.now();

  let browser;
  try {
    browser = await puppeteer.launch({ 
      headless: 'new',
      args: ['--no-sandbox', '--disable-setuid-sandbox']
    });
    const page = await browser.newPage();
    
    // Set user agent to look like a real browser
    await page.setUserAgent('Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36');
    
    // Trading212 instrument page URL pattern (adjust if needed based on actual site structure)
    // Example: https://www.trading212.com/trading-instruments/AAPL
    const url = `https://www.trading212.com/trading-instruments/${symbol}`;
    
    await page.goto(url, { waitUntil: 'networkidle2', timeout: 15000 });
    
    // Extract price data from page (selectors may need adjustment based on actual DOM)
    const data = await page.evaluate(() => {
      // These selectors are placeholders - you'll need to inspect Trading212's actual page structure
      // Common patterns: look for elements with classes like 'price', 'ticker-price', 'instrument-price', etc.
      
      // Try multiple possible selectors
      const priceSelectors = [
        '.instrument-price',
        '[data-qa="instrument-price"]',
        '.ticker-price',
        '.price-value',
        'span[class*="price"]'
      ];
      
      const changeSelectors = [
        '.price-change',
        '[data-qa="price-change"]',
        '.change-value',
        'span[class*="change"]'
      ];
      
      let price = null;
      let change = null;
      let percent = null;
      
      // Try to find price
      for (const selector of priceSelectors) {
        const el = document.querySelector(selector);
        if (el && el.textContent) {
          const text = el.textContent.trim().replace(/[^0-9.-]/g, '');
          const parsed = parseFloat(text);
          if (!isNaN(parsed)) {
            price = parsed;
            break;
          }
        }
      }
      
      // Try to find change
      for (const selector of changeSelectors) {
        const el = document.querySelector(selector);
        if (el && el.textContent) {
          const text = el.textContent.trim();
          // Extract number and percentage if present
          const numMatch = text.match(/([+-]?[0-9.]+)/);
          const pctMatch = text.match(/([+-]?[0-9.]+)%/);
          
          if (numMatch) change = parseFloat(numMatch[1]);
          if (pctMatch) percent = parseFloat(pctMatch[1]);
          
          if (change !== null) break;
        }
      }
      
      return { price, change, percent };
    });
    
    await browser.close();
    
    // Validate we got at least a price
    if (data.price === null || data.price === 0) {
      throw new Error(`No valid price found for ${symbol}`);
    }
    
    // If change/percent not found, calculate from previous cache if available
    if (data.change === null && cache[symbol]) {
      const prevPrice = cache[symbol].price;
      data.change = data.price - prevPrice;
      data.percent = prevPrice !== 0 ? (data.change / prevPrice) * 100 : 0;
    } else if (data.change === null) {
      data.change = 0;
      data.percent = 0;
    }
    
    // Update cache
    cache[symbol] = {
      symbol,
      name: symbol, // Could enhance by scraping full name
      price: data.price,
      change: data.change || 0,
      percent: data.percent || 0,
      timestamp: Date.now()
    };
    
    console.log(`[scraper] ${symbol}: price=${data.price} change=${data.change} percent=${data.percent}`);
    return cache[symbol];
    
  } catch (error) {
    if (browser) await browser.close();
    console.error(`[scraper] error fetching ${symbol}:`, error.message);
    
    // Return stale cache if available
    if (cache[symbol]) {
      console.log(`[scraper] returning stale cache for ${symbol}`);
      return cache[symbol];
    }
    
    throw error;
  }
}

// GET /quotes?symbols=AAPL,MSFT,TSLA
app.get('/quotes', async (req, res) => {
  const symbolsParam = req.query.symbols;
  
  if (!symbolsParam) {
    return res.status(400).json({ error: 'Missing symbols parameter' });
  }
  
  const symbols = symbolsParam.split(',').map(s => s.trim().toUpperCase()).filter(Boolean);
  
  if (symbols.length === 0) {
    return res.status(400).json({ error: 'No valid symbols provided' });
  }
  
  console.log(`[scraper] request for symbols: ${symbols.join(', ')}`);
  
  try {
    const results = [];
    
    // Scrape sequentially to respect rate limits
    for (const symbol of symbols) {
      try {
        const data = await scrapeSymbol(symbol);
        results.push(data);
      } catch (err) {
        console.error(`[scraper] failed to scrape ${symbol}:`, err.message);
        // Continue with other symbols
      }
    }
    
    if (results.length === 0) {
      return res.status(500).json({ error: 'Failed to scrape any symbols' });
    }
    
    res.json(results);
    
  } catch (error) {
    console.error('[scraper] error:', error);
    res.status(500).json({ error: error.message });
  }
});

// Health check
app.get('/health', (req, res) => {
  res.json({ 
    status: 'ok', 
    cachedSymbols: Object.keys(cache).length,
    cache: Object.keys(cache).map(sym => ({
      symbol: sym,
      age: Date.now() - cache[sym].timestamp
    }))
  });
});

app.listen(PORT, 'localhost', () => {
  console.log(`Trading212 scraper running on http://localhost:${PORT}`);
  console.log(`Usage: GET /quotes?symbols=AAPL,MSFT,TSLA`);
  console.log(`Cache TTL: ${CACHE_TTL_MS}ms`);
  console.log(`Min scrape interval: ${MIN_SCRAPE_INTERVAL_MS}ms`);
});
