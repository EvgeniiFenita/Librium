const LIMIT = 40;
let offset = 0;
let totalFound = 0;
let isLoading = false;
let currentQuery = {};
let searchField = 'title';
let debounceTimer = null;
let pollTimer = null;

// --- UTILITIES ---
function hashColor(str) {
  let h = 0;
  for (let i = 0; i < str.length; i++) h = Math.imul(31, h) + str.charCodeAt(i) | 0;
  const hue = Math.abs(h) % 360;
  return `hsl(${hue}, 40%, 25%)`;
}

function hashColor2(str) {
  let h = 0;
  for (let i = 0; i < str.length; i++) h = Math.imul(31, h) + str.charCodeAt(i) | 0;
  const hue = (Math.abs(h) % 360 + 30) % 360;
  return `hsl(${hue}, 50%, 35%)`;
}

/**
 * Format authors list to "Author A, Author B" or "Author A + N".
 */
function formatAuthor(authors) {
  if (!authors || !authors.length) return '';
  const names = authors.map(a => [a.lastName, a.firstName].filter(Boolean).join(' '));
  if (names.length <= 2) return names.join(', ');
  return `${names[0]} + ${names.length - 1}`;
}

function formatSize(bytes) {
  if (!bytes) return '';
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(0)} KB`;
  return `${(bytes / 1024 / 1024).toFixed(1)} MB`;
}

/**
 * Enhanced fetch wrapper with error body parsing.
 */
async function api(path, options = {}) {
  const res = await fetch(path, options);
  if (!res.ok) {
    let errorMessage = `HTTP ${res.status}`;
    try {
      const body = await res.json();
      if (body.error) errorMessage = body.error;
    } catch (e) { /* ignore parse error */ }
    throw new Error(errorMessage);
  }
  return res.json();
}

function escapeHtml(str) {
  return String(str ?? '').replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}

function showLoadingMore(show) {
  document.getElementById('loading-more').classList.toggle('hidden', !show);
}

// --- PLACEHOLDERS ---
function createPlaceholderSvg(title) {
  const c1 = hashColor(title || 'book');
  const c2 = hashColor2(title || 'book');
  return `
    <div style="width:100%;height:100%;background:linear-gradient(135deg,${c1},${c2});
                display:flex;align-items:center;justify-content:center;">
      <svg viewBox="0 0 24 24" width="40%" fill="rgba(255,255,255,0.4)">
        <path d="M6 2h12a2 2 0 0 1 2 2v16a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2zm0 2v16h12V4H6zm2 3h8v2H8V7zm0 4h8v2H8v-2zm0 4h5v2H8v-2z"/>
      </svg>
    </div>`;
}

// --- BOOK GRID ---
function renderBookCard(book) {
  const card = document.createElement('div');
  card.className = 'book-card';
  card.dataset.id = book.id;

  // Use generic extension-less URL, server handles detection
  const coverUrl = `/covers/${book.id}/cover`;
  const authorText = formatAuthor(book.authors);

  card.innerHTML = `
    <div class="book-cover-wrap">
      <img src="${coverUrl}" alt="" loading="lazy"
           onerror="this.style.display='none';this.nextElementSibling.style.display='flex'">
      <div class="cover-placeholder" style="display:none">${createPlaceholderSvg(book.title)}</div>
    </div>
    <div class="book-card-info">
      <div class="book-card-title">${escapeHtml(book.title)}</div>
      <div class="book-card-author">${escapeHtml(authorText)}</div>
    </div>`;

  card.addEventListener('click', () => openModal(book.id));
  return card;
}

async function loadBooks(reset = false) {
  if (isLoading) return;
  
  if (!reset && totalFound > 0 && offset >= totalFound) {
    return;
  }

  isLoading = true;
  showLoadingMore(!reset);

  if (reset) {
    offset = 0;
    totalFound = 0;
    document.getElementById('book-grid').innerHTML = '';
  }

  try {
    const params = new URLSearchParams({ limit: LIMIT, offset: offset });
    if (currentQuery.value) {
      params.set(currentQuery.field, currentQuery.value);
    }

    const data = await api(`/api/books?${params}`);
    const books = data.books || [];
    
    if (books.length > 0) {
        hideImportOverlay();
    }

    if (reset || data.totalFound > 0) {
      totalFound = data.totalFound || 0;
    }
    
    offset += books.length;

    const grid = document.getElementById('book-grid');
    books.forEach(book => grid.appendChild(renderBookCard(book)));

    document.getElementById('empty-state').classList.toggle(
      'hidden', grid.children.length > 0 || offset > 0
    );

    if (books.length < LIMIT) {
      totalFound = offset; 
    }
  } catch (e) {
    console.error('Failed to load books:', e);
  } finally {
    isLoading = false;
    showLoadingMore(false);
  }
}

// --- INFINITE SCROLL ---
function setupInfiniteScroll() {
  const sentinel = document.getElementById('sentinel');
  const observer = new IntersectionObserver((entries) => {
    if (entries[0].isIntersecting && !isLoading) {
        if (totalFound === 0 || offset < totalFound) {
            loadBooks(false);
        }
    }
  }, { rootMargin: '400px' });
  observer.observe(sentinel);
}

// --- SEARCH ---
function setupSearch() {
  const input = document.getElementById('search-input');

  input.addEventListener('input', () => {
    clearTimeout(debounceTimer);
    debounceTimer = setTimeout(() => doSearch(), 400);
  });

  input.addEventListener('keydown', (e) => {
    if (e.key === 'Enter') {
      clearTimeout(debounceTimer);
      doSearch();
    }
  });

  document.querySelectorAll('.filter-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      document.querySelectorAll('.filter-btn').forEach(b => b.classList.remove('active'));
      btn.classList.add('active');
      searchField = btn.dataset.field;
      input.placeholder = `Search by ${btn.textContent.toLowerCase()}...`;
      if (input.value.trim()) doSearch();
    });
  });
}

function doSearch() {
  const value = document.getElementById('search-input').value.trim();
  currentQuery = { field: searchField, value };
  loadBooks(true);
}

// --- MODAL ---
async function openModal(bookId) {
  const modal = document.getElementById('book-modal');
  modal.classList.remove('hidden');

  document.getElementById('modal-title').textContent = 'Loading...';
  document.getElementById('modal-annotation').classList.add('hidden');
  document.getElementById('modal-authors').innerHTML = '';
  document.getElementById('modal-series').classList.add('hidden');
  document.getElementById('modal-genre').classList.add('hidden');
  document.getElementById('modal-details').textContent = '';

  const img = document.getElementById('modal-cover');
  const ph = document.getElementById('modal-cover-placeholder');
  img.style.display = 'none';
  ph.classList.add('hidden');

  const btn = document.getElementById('download-btn');
  btn.style.display = 'none';

  try {
    const data = await api(`/api/books/${bookId}`);
    populateModal(data);
  } catch (e) {
    document.getElementById('modal-title').textContent = e.message || 'Loading error';
  }
}

function populateModal(book) {
  document.getElementById('modal-title').textContent = book.title || 'Untitled';

  const img = document.getElementById('modal-cover');
  const ph = document.getElementById('modal-cover-placeholder');
  
  if (book.coverUrl) {
    img.src = book.coverUrl;
    img.style.display = 'block';
    img.onerror = () => {
      img.style.display = 'none';
      ph.innerHTML = createPlaceholderSvg(book.title);
      ph.classList.remove('hidden');
      ph.style.display = 'flex';
    };
  } else {
    ph.innerHTML = createPlaceholderSvg(book.title);
    ph.classList.remove('hidden');
    ph.style.display = 'flex';
  }

  const authors = formatAuthor(book.authors) || '—';
  document.getElementById('modal-authors').innerHTML = `<strong>Author:</strong> ${escapeHtml(authors)}`;

  const seriesEl = document.getElementById('modal-series');
  if (book.series) {
    const snum = book.seriesNumber ? ` #${book.seriesNumber}` : '';
    seriesEl.innerHTML = `<strong>Series:</strong> ${escapeHtml(book.series)}${snum}`;
    seriesEl.classList.remove('hidden');
  }

  const genreEl = document.getElementById('modal-genre');
  if (book.genres && book.genres.length) {
    genreEl.innerHTML = `<strong>Genre:</strong> ${escapeHtml(book.genres.join(', '))}`;
    genreEl.classList.remove('hidden');
  }

  const details = [
    book.language && `Language: ${book.language.toUpperCase()}`,
    book.publishYear && `Year: ${book.publishYear}`,
    book.size && `Size: ${formatSize(book.size)}`,
    book.ext && `Format: ${book.ext.toUpperCase()}`,
  ].filter(Boolean).join(' · ');
  document.getElementById('modal-details').textContent = details;

  const annEl = document.getElementById('modal-annotation');
  if (book.annotation) {
    annEl.textContent = book.annotation;
    annEl.classList.remove('hidden');
  }

  const btn = document.getElementById('download-btn');
  btn.style.display = 'inline-block';
  btn.onclick = () => downloadBook(book);
}

/**
 * Trigger book download safely.
 */
async function downloadBook(book) {
  const btn = document.getElementById('download-btn');
  const originalText = btn.innerHTML;
  
  try {
    btn.disabled = true;
    btn.innerHTML = '⏳ Processing...';
    
    const res = await fetch(`/api/download/${book.id}`);
    if (!res.ok) {
      let errMsg = 'Download failed';
      try {
        const errBody = await res.json();
        if (errBody.error) errMsg = errBody.error;
      } catch (e) {
        const text = await res.text();
        if (text) errMsg = text;
      }
      throw new Error(errMsg);
    }
    
    const blob = await res.blob();
    const url = window.URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.href = url;
    
    // Format: "Author - Title.fb2"
    const authorText = formatAuthor(book.authors);
    let filename = authorText ? `${authorText} - ${book.title}` : book.title;
    filename = (filename || 'book') + '.' + (book.ext || 'fb2');
    
    // Clean up invalid filename characters
    filename = filename.replace(/[\\\/:*?"<>|]/g, '_');

    link.download = filename;
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
    window.URL.revokeObjectURL(url);
  } catch (e) {
    alert(`Download error: ${e.message}`);
  } finally {
    btn.disabled = false;
    btn.innerHTML = originalText;
  }
}

function closeModal() {
  document.getElementById('book-modal').classList.add('hidden');
}

function setupModal() {
  document.getElementById('modal-close').addEventListener('click', closeModal);
  document.querySelector('.modal-backdrop').addEventListener('click', closeModal);
  document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape') closeModal();
  });
}

// --- IMPORT & PROGRESS ---
async function checkInitialStatus() {
  try {
    const status = await api('/api/status');

    if (status.state === 'importing' || status.state === 'upgrading') {
      showImportOverlay(status.state === 'upgrading' ? 'Updating library...' : 'Importing library...');
      startPolling();
      return;
    }

    if (status.state === 'ready') {
      const stats = await api('/api/stats');
      const bookCount = stats.total_books ?? 0;
      
      if (bookCount === 0) {
        showImportOverlay('First run: importing library...');
        fetch('/api/import', { method: 'POST' }).catch(console.error);
        startPolling();
      } else {
        updateStatsBadge(stats);
        hideImportOverlay();
        loadBooks(true);
      }
    } else {
        setTimeout(checkInitialStatus, 2000);
    }
  } catch (e) {
    setTimeout(checkInitialStatus, 2000);
  }
}

function showImportOverlay(title) {
  document.getElementById('overlay-title').textContent = title;
  document.getElementById('import-overlay').classList.remove('hidden');
}

function hideImportOverlay() {
  document.getElementById('import-overlay').classList.add('hidden');
}

function startPolling() {
  clearInterval(pollTimer);
  pollTimer = setInterval(async () => {
    try {
      const status = await api('/api/status');
      updateProgressBar(status.progress);

      if (status.state === 'ready') {
        clearInterval(pollTimer);
        const stats = await api('/api/stats');
        updateStatsBadge(stats);
        hideImportOverlay();
        resetUpgradeButton();
        loadBooks(true);
      }
    } catch (e) {
        console.error('Polling error:', e);
    }
  }, 1000);
}

function updateProgressBar(progress) {
  const { processed = 0, total = 0 } = progress || {};
  const pct = total > 0 ? Math.round((processed / total) * 100) : 0;
  document.getElementById('progress-bar').style.width = `${pct}%`;
  document.getElementById('progress-text').textContent =
    total > 0
      ? `${processed.toLocaleString()} / ${total.toLocaleString()} books (${pct}%)`
      : 'Preparing...';
}

function setupHeader() {
  document.getElementById('upgrade-btn').addEventListener('click', async () => {
    const btn = document.getElementById('upgrade-btn');
    btn.disabled = true;
    btn.querySelector('.btn-text').classList.add('hidden');
    btn.querySelector('.btn-spinner').classList.remove('hidden');

    try {
      await fetch('/api/upgrade', { method: 'POST' });
      startPolling();
    } catch (e) {
      resetUpgradeButton();
    }
  });
}

function resetUpgradeButton() {
    const btn = document.getElementById('upgrade-btn');
    btn.disabled = false;
    btn.querySelector('.btn-text').classList.remove('hidden');
    btn.querySelector('.btn-spinner').classList.add('hidden');
}

function updateStatsBadge(stats) {
  const badge = document.getElementById('stats-badge');
  const count = stats.total_books ?? 0;
  badge.textContent = count > 0 ? `${count.toLocaleString()} books` : '';
}

// --- INIT ---
document.addEventListener('DOMContentLoaded', () => {
  setupSearch();
  setupModal();
  setupInfiniteScroll();
  setupHeader();
  checkInitialStatus();
});

