// Renders Markdown the way GitHub does: GFM tables, task lists, strikethrough,
// autolinks, alerts, footnotes, heading anchors, math, Mermaid diagrams, and
// highlighted code. The Qt side drives it through window.mdview.
'use strict';

const content = document.getElementById('content');
const toc = document.getElementById('toc');

const escapeHtml = (s) => String(s).replace(/[&<>"']/g, (c) =>
  ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));

const md = new marked.Marked({ gfm: true, breaks: false });
md.use(
  markedGfmHeadingId.gfmHeadingId(),
  markedAlert(),
  markedFootnote({ description: 'Footnotes' }),
  markedKatex({ throwOnError: false, output: 'htmlAndMathml' }),
  {
    renderer: {
      // Mermaid and math fences become diagrams and formulas, as on GitHub.
      // Returning false hands every other block to marked's own renderer.
      code(token) {
        const lang = (token.lang || '').trim().split(/\s+/)[0].toLowerCase();
        if (lang === 'mermaid')
          return `<pre class="mermaid">${escapeHtml(token.text)}</pre>\n`;
        if (lang === 'math') {
          try {
            return `<div class="math-display">${katex.renderToString(token.text, { displayMode: true, throwOnError: false })}</div>\n`;
          } catch (e) {
            return false;
          }
        }
        return false;
      },
    },
  },
);

// GitHub shows YAML front matter as a one-row table instead of a stray rule.
function splitFrontMatter(text) {
  const match = /^---[ \t]*\r?\n([\s\S]*?)\r?\n(?:---|\.\.\.)[ \t]*(?:\r?\n|$)/.exec(text);
  if (!match) return { front: '', body: text };
  const pairs = [];
  for (const line of match[1].split(/\r?\n/)) {
    const pair = /^([A-Za-z0-9_.-]+)\s*:\s*(.*)$/.exec(line);
    if (pair) pairs.push([pair[1], pair[2].replace(/^["']|["']$/g, '')]);
    else if (pairs.length && line.trim()) pairs[pairs.length - 1][1] += ' ' + line.trim();
  }
  if (!pairs.length) return { front: '', body: text.slice(match[0].length) };
  const head = pairs.map(([k]) => `<th>${escapeHtml(k)}</th>`).join('');
  const row = pairs.map(([, v]) => `<td>${escapeHtml(v)}</td>`).join('');
  return {
    front: `<table class="front-matter"><thead><tr>${head}</tr></thead><tbody><tr>${row}</tr></tbody></table>`,
    body: text.slice(match[0].length),
  };
}

// Relative images and links point next to the Markdown file, not into the
// app's own resources.
function absolutize(root, baseUrl) {
  if (!baseUrl) return;
  const fix = (value) => {
    if (!value || value.startsWith('#') || /^[a-z][a-z0-9+.-]*:/i.test(value)) return value;
    try { return new URL(value, baseUrl).href; } catch (e) { return value; }
  };
  // Chromium won't let the bundled page load file:// media, so local images
  // and videos come through the app's own mdview-file: scheme.
  const media = (value) => fix(value).replace(/^file:\/\//i, 'mdview-file://');
  for (const el of root.querySelectorAll('img[src], video[src], audio[src], source[src], track[src]'))
    el.setAttribute('src', media(el.getAttribute('src')));
  for (const el of root.querySelectorAll('video[poster]'))
    el.setAttribute('poster', media(el.getAttribute('poster')));
  for (const el of root.querySelectorAll('a[href]'))
    el.setAttribute('href', fix(el.getAttribute('href')));
  for (const el of root.querySelectorAll('source[srcset], img[srcset]'))
    el.setAttribute('srcset', el.getAttribute('srcset').split(',').map((part) => {
      const [url, ...rest] = part.trim().split(/\s+/);
      return [media(url), ...rest].join(' ');
    }).join(', '));
}

const LINK_ICON = '<svg class="octicon octicon-link" viewBox="0 0 16 16" width="16" height="16" aria-hidden="true"><path d="m7.775 3.275 1.25-1.25a3.5 3.5 0 1 1 4.95 4.95l-2.5 2.5a3.5 3.5 0 0 1-4.95 0 .751.751 0 0 1 .018-1.042.751.751 0 0 1 1.042-.018 1.998 1.998 0 0 0 2.83 0l2.5-2.5a2.002 2.002 0 0 0-2.83-2.83l-1.25 1.25a.751.751 0 0 1-1.042-.018.751.751 0 0 1-.018-1.042Zm-4.69 9.64a1.998 1.998 0 0 0 2.83 0l1.25-1.25a.751.751 0 0 1 1.042.018.751.751 0 0 1 .018 1.042l-1.25 1.25a3.5 3.5 0 1 1-4.95-4.95l2.5-2.5a3.5 3.5 0 0 1 4.95 0 .751.751 0 0 1-.018 1.042.751.751 0 0 1-1.042.018 1.998 1.998 0 0 0-2.83 0l-2.5 2.5a1.998 1.998 0 0 0 0 2.83Z"></path></svg>';

// The classes and anchors GitHub's stylesheet expects but marked leaves out.
function decorate(root) {
  for (const h of root.querySelectorAll('h1[id], h2[id], h3[id], h4[id], h5[id], h6[id]')) {
    const a = document.createElement('a');
    a.className = 'anchor';
    a.href = '#' + h.id;
    a.setAttribute('aria-hidden', 'true');
    a.innerHTML = LINK_ICON;
    h.prepend(a);
  }
  for (const box of root.querySelectorAll('li > input[type="checkbox"]:first-child, li > p:first-child > input[type="checkbox"]:first-child')) {
    box.classList.add('task-list-item-checkbox');
    const li = box.closest('li');
    li.classList.add('task-list-item');
    li.parentElement.classList.add('contains-task-list');
  }
  for (const code of root.querySelectorAll('pre > code')) {
    const lang = [...code.classList].find((c) => c.startsWith('language-'));
    const name = lang && lang.slice(9);
    code.parentElement.classList.add('notranslate');
    if (name && hljs.getLanguage(name)) {
      try { hljs.highlightElement(code); } catch (e) { /* plain text is fine */ }
    }
  }
  for (const table of root.querySelectorAll('table')) {
    // GitHub scrolls wide tables inside the column instead of overflowing it.
    table.style.display = 'block';
    table.style.overflow = 'auto';
    table.style.width = 'max-content';
    table.style.maxWidth = '100%';
  }
}

// Mermaid is a large script, so it only loads for documents with diagrams.
let mermaidReady = null;
let theme = { mode: 'dark', colors: {} };

function loadMermaid() {
  if (!mermaidReady) {
    mermaidReady = new Promise((resolve, reject) => {
      const script = document.createElement('script');
      script.src = 'vendor/mermaid.js';
      script.onload = () => resolve(window.mermaid);
      script.onerror = () => reject(new Error('Mermaid failed to load'));
      document.head.appendChild(script);
    });
  }
  return mermaidReady;
}

function configureMermaid(mermaid) {
  const c = theme.colors;
  mermaid.initialize({
    startOnLoad: false,
    securityLevel: 'strict',
    theme: 'base',
    fontFamily: '-apple-system, BlinkMacSystemFont, "Segoe UI", "Noto Sans", Helvetica, Arial, sans-serif',
    themeVariables: {
      darkMode: theme.mode !== 'light',
      background: c.background,
      primaryColor: c.lighter_background || c.selection,
      primaryTextColor: c.foreground,
      primaryBorderColor: c.accent,
      secondaryColor: c.dark_background,
      tertiaryColor: c.background,
      lineColor: c.dark_foreground || c.foreground,
      textColor: c.foreground,
      mainBkg: c.lighter_background || c.selection,
      nodeBorder: c.accent,
      clusterBkg: c.dark_background,
      clusterBorder: c.muted,
      noteBkgColor: c.lighter_background,
      noteTextColor: c.foreground,
      edgeLabelBackground: c.background,
    },
  });
}

async function drawDiagrams() {
  const nodes = [...content.querySelectorAll('pre.mermaid')];
  if (!nodes.length) return;
  let mermaid;
  try {
    mermaid = await loadMermaid();
  } catch (e) {
    for (const n of nodes) { n.classList.add('failed'); n.textContent = e.message; }
    return;
  }
  configureMermaid(mermaid);
  for (const n of nodes) {
    n.removeAttribute('data-processed');
    n.textContent = n.dataset.source;
  }
  try {
    await mermaid.run({ nodes, suppressErrors: true });
  } catch (e) { /* each failed diagram shows its own error below */ }
  for (const n of nodes) {
    // A diagram Mermaid can't parse shows its source, marked as broken,
    // rather than Mermaid's own bomb graphic.
    if (!n.querySelector('svg') || n.querySelector('svg[aria-roledescription="error"]')) {
      n.classList.add('failed');
      n.textContent = n.dataset.source;
    }
  }
}

function buildToc() {
  const headings = [...content.querySelectorAll('h1[id], h2[id], h3[id], h4[id], h5[id], h6[id]')].filter((h) => !h.classList.contains('sr-only'));
  toc.innerHTML = '';
  const title = document.createElement('div');
  title.className = 'title';
  title.textContent = window.mdviewStrings.outline;
  toc.appendChild(title);
  for (const h of headings) {
    const a = document.createElement('a');
    a.href = '#' + h.id;
    a.dataset.level = h.tagName.slice(1);
    a.textContent = h.textContent.trim();
    a.title = a.textContent;
    toc.appendChild(a);
  }
  markCurrentHeading();
}

function markCurrentHeading() {
  if (toc.hidden) return;
  const headings = [...content.querySelectorAll('h1[id], h2[id], h3[id], h4[id], h5[id], h6[id]')].filter((h) => !h.classList.contains('sr-only'));
  let current = headings[0];
  for (const h of headings) {
    if (h.getBoundingClientRect().top <= 80) current = h;
    else break;
  }
  for (const a of toc.querySelectorAll('a'))
    a.classList.toggle('current', !!current && a.getAttribute('href') === '#' + current.id);
}

function scrollToId(id) {
  const target = document.getElementById(id) || document.querySelector(`[name="${CSS.escape(id)}"]`);
  if (target) target.scrollIntoView({ block: 'start' });
}

// In-page links scroll here; every other link is left to the Qt side, which
// opens Markdown files in the viewer and everything else in its own app.
document.addEventListener('click', (event) => {
  const a = event.target.closest('a[href]');
  if (!a) return;
  const href = a.getAttribute('href');
  if (href.startsWith('#')) {
    event.preventDefault();
    scrollToId(decodeURIComponent(href.slice(1)));
  }
});

window.addEventListener('scroll', markCurrentHeading, { passive: true });

let renderGeneration = 0;

window.mdviewStrings = { outline: 'Outline', empty: 'This file is empty.' };

window.mdview = {
  setStrings(strings) {
    Object.assign(window.mdviewStrings, strings);
    if (!toc.hidden) buildToc();
  },

  setTheme(next) {
    theme = { mode: next.mode === 'light' ? 'light' : 'dark', colors: next.colors || {} };
    const c = theme.colors;
    const root = document.documentElement.style;
    const set = (name, value) => { if (value) root.setProperty(name, value); };
    set('--mv-scheme', theme.mode);
    set('--mv-bg', c.background);
    set('--mv-fg', c.foreground);
    // Derived shades work for any theme, whatever extra keys it defines.
    root.setProperty('--mv-bg-muted', 'color-mix(in srgb, var(--mv-fg) 6%, var(--mv-bg))');
    root.setProperty('--mv-fg-muted', 'color-mix(in srgb, var(--mv-fg) 62%, var(--mv-bg))');
    root.setProperty('--mv-border', 'color-mix(in srgb, var(--mv-fg) 22%, var(--mv-bg))');
    root.setProperty('--mv-border-muted', 'color-mix(in srgb, var(--mv-fg) 15%, var(--mv-bg))');
    root.setProperty('--mv-code-bg', 'color-mix(in srgb, var(--mv-fg) 12%, transparent)');
    set('--mv-accent', c.accent);
    set('--mv-blue', c.blue || c.accent);
    set('--mv-red', c.red);
    set('--mv-green', c.green);
    set('--mv-yellow', c.yellow);
    set('--mv-magenta', c.magenta);
    set('--mv-cyan', c.cyan);
    set('--mv-orange', c.orange || c.yellow);
    root.setProperty('--mv-mark', 'color-mix(in srgb, var(--mv-yellow) 25%, transparent)');
    set('--mv-selection', c.selection);
    root.setProperty('--mv-radius', `${Math.max(0, next.radius | 0)}px`);
    if (content.querySelector('pre.mermaid')) drawDiagrams();
  },

  // Renders a document. keepScroll holds the reading position across live
  // reloads, and a reader at the very end stays at the end as the file grows.
  async render(text, baseUrl, keepScroll) {
    const generation = ++renderGeneration;
    const atEnd = window.innerHeight + window.scrollY >= document.body.scrollHeight - 4 && window.scrollY > 0;
    const y = keepScroll ? window.scrollY : 0;

    markedGfmHeadingId.resetHeadings();
    const { front, body } = splitFrontMatter(text);
    const html = front + md.parse(body);
    content.innerHTML = DOMPurify.sanitize(html, { FORBID_TAGS: ['style', 'form'] });
    // Mermaid replaces each block with its drawing, so keep the source for
    // redrawing when the theme changes.
    for (const n of content.querySelectorAll('pre.mermaid')) n.dataset.source = n.textContent;
    if (!content.textContent.trim() && !content.querySelector('img, svg, video, table'))
      content.innerHTML = `<p class="empty">${escapeHtml(window.mdviewStrings.empty)}</p>`;
    absolutize(content, baseUrl);
    decorate(content);
    if (!toc.hidden) buildToc();

    const restore = () => {
      if (generation !== renderGeneration) return;
      window.scrollTo(0, keepScroll && atEnd ? document.body.scrollHeight : y);
    };
    restore();
    await drawDiagrams();
    restore();
    // Images change the height as they arrive.
    for (const img of content.querySelectorAll('img'))
      if (!img.complete) img.addEventListener('load', restore, { once: true });
  },

  scrollBy(dy) { window.scrollBy(0, dy); },
  top() { window.scrollTo(0, 0); },
  bottom() { window.scrollTo(0, document.body.scrollHeight); },
  pageBy(pages) { window.scrollBy(0, pages * window.innerHeight * 0.9); },

  toggleToc() {
    toc.hidden = !toc.hidden;
    document.body.classList.toggle('toc-open', !toc.hidden);
    if (!toc.hidden) buildToc();
    return !toc.hidden;
  },
};
