/**
 * EdgeDrop Official Product Website & Interactive Notch Playground
 * Zero dependencies. Pure vanilla ES6. 60fps pointer physics.
 */

(() => {
  'use strict';

  // ── 1. Color Theme Mode (Porcelain / Obsidian) ───────────────────────────

  const themeToggleBtn = document.getElementById('theme-toggle');
  const themeLabel = document.getElementById('theme-label-text');
  const htmlEl = document.documentElement;

  function initTheme() {
    const savedTheme = localStorage.getItem('edgedrop-site-theme');
    if (savedTheme === 'obsidian' || (!savedTheme && window.matchMedia('(prefers-color-scheme: dark)').matches)) {
      setTheme('obsidian');
    } else {
      setTheme('porcelain');
    }
  }

  function setTheme(theme) {
    htmlEl.setAttribute('data-theme', theme);
    if (themeLabel) {
      themeLabel.textContent = theme === 'obsidian' ? 'Obsidian' : 'Porcelain';
    }
    localStorage.setItem('edgedrop-site-theme', theme);
  }

  if (themeToggleBtn) {
    themeToggleBtn.addEventListener('click', () => {
      const current = htmlEl.getAttribute('data-theme');
      setTheme(current === 'obsidian' ? 'porcelain' : 'obsidian');
    });
  }

  initTheme();

  // ── 2. Smart OS Detection & Download CTA Setup ───────────────────────────

  const primaryBtn = document.getElementById('primary-cta-btn');
  const primaryLabel = document.getElementById('primary-cta-label');
  const primarySubtext = document.getElementById('primary-cta-subtext');
  const mobileNoticeBox = document.getElementById('mobile-notice-box');
  const copyPageLinkBtn = document.getElementById('copy-page-link-btn');
  const copyLinkText = document.getElementById('copy-link-text');
  const checksumCommandCode = document.getElementById('checksum-command-code');

  const RELEASE_BASE = 'https://github.com/block-plant/edgedrop/releases/latest/download';
  const URL_MAC = `${RELEASE_BASE}/EdgeDrop-macOS-arm64.dmg`;
  const URL_WIN = `${RELEASE_BASE}/EdgeDrop-Windows-x64.exe`;
  const URL_WIN_PORTABLE = `${RELEASE_BASE}/EdgeDrop-Windows-x64-Portable.zip`;
  const URL_LINUX = `${RELEASE_BASE}/EdgeDrop-Linux-x86_64.AppImage`;

  function detectClientOS() {
    const ua = navigator.userAgent || '';
    const platform = (navigator.userAgentData && navigator.userAgentData.platform) || navigator.platform || '';

    // Mobile platforms
    if (/android/i.test(ua)) return 'android';
    if (/ipad|iphone|ipod/i.test(ua) || (platform === 'MacIntel' && navigator.maxTouchPoints > 1)) return 'ios';

    // Desktop platforms
    if (/mac/i.test(platform) || /macintosh|mac os x/i.test(ua)) return 'mac';
    if (/win/i.test(platform) || /windows/i.test(ua)) return 'win';
    if (/linux/i.test(platform) || /linux/i.test(ua)) return 'linux';

    return 'unknown';
  }

  function setupDownloadCTA() {
    const os = detectClientOS();

    if (os === 'ios' || os === 'android') {
      if (mobileNoticeBox) mobileNoticeBox.style.display = 'block';
      if (primaryBtn) {
        primaryBtn.href = URL_MAC;
        if (primaryLabel) primaryLabel.textContent = 'Desktop Installers';
        if (primarySubtext) primarySubtext.textContent = 'macOS · Windows · Linux';
      }
    } else if (os === 'win') {
      if (primaryBtn) {
        primaryBtn.href = URL_WIN;
        if (primaryLabel) primaryLabel.textContent = 'Download for Windows';
        if (primarySubtext) primarySubtext.textContent = 'Windows 10/11 x64 · .exe';
      }
      if (checksumCommandCode) {
        checksumCommandCode.textContent = 'certutil -hashfile EdgeDrop-Windows-x64.exe SHA256';
      }
    } else if (os === 'linux') {
      if (primaryBtn) {
        primaryBtn.href = URL_LINUX;
        if (primaryLabel) primaryLabel.textContent = 'Download for Linux';
        if (primarySubtext) primarySubtext.textContent = 'x86_64 · .AppImage';
      }
      if (checksumCommandCode) {
        checksumCommandCode.textContent = 'sha256sum -c SHA256SUMS.txt';
      }
    } else {
      // Default to macOS
      if (primaryBtn) {
        primaryBtn.href = URL_MAC;
        if (primaryLabel) primaryLabel.textContent = 'Download for macOS';
        if (primarySubtext) primarySubtext.textContent = 'Apple Silicon · .dmg';
      }
      if (checksumCommandCode) {
        checksumCommandCode.textContent = 'shasum -a 256 -c SHA256SUMS.txt';
      }
    }
  }

  setupDownloadCTA();

  // Mobile copy link button
  if (copyPageLinkBtn) {
    copyPageLinkBtn.addEventListener('click', async () => {
      try {
        await navigator.clipboard.writeText(window.location.href);
        if (copyLinkText) copyLinkText.textContent = 'Link copied!';
        setTimeout(() => {
          if (copyLinkText) copyLinkText.textContent = 'Copy link to clipboard';
        }, 2200);
      } catch {
        if (copyLinkText) copyLinkText.textContent = 'https://block-plant.github.io/edgedrop/';
      }
    });
  }

  // ── 3. Interactive Miniature Notch Simulation (Physics & Drag) ───────────

  const viewport = document.getElementById('desktop-viewport');
  const notchWrapper = document.getElementById('sim-notch-wrapper');
  const dragHandle = document.getElementById('notch-drag-handle');

  // Adaptive Wallpaper Tone Definitions (Live retinting of Adaptive vibe)
  const WALLPAPER_TONES = {
    alpine: {
      bg: 'hsl(212, 45%, 98%)',
      ink: '#141518',
      elevated: 'hsl(212, 35%, 93%)',
      border: 'hsla(212, 40%, 20%, 0.12)',
      accent: '#2563EB'
    },
    pines: {
      bg: 'hsl(210, 24%, 12%)',
      ink: '#F4F4F5',
      elevated: 'hsl(210, 20%, 18%)',
      border: 'hsla(210, 30%, 80%, 0.12)',
      accent: '#2DD4BF'
    },
    sand: {
      bg: 'hsl(40, 30%, 97%)',
      ink: '#18181B',
      elevated: 'hsl(40, 25%, 92%)',
      border: 'hsla(40, 30%, 20%, 0.12)',
      accent: '#B45309'
    }
  };

  let currentWallpaper = 'alpine';
  let currentVibe = 'adaptive';
  let currentEdge = 'right';

  function applyAdaptiveTone() {
    if (currentVibe !== 'adaptive' || !notchWrapper) return;
    const tone = WALLPAPER_TONES[currentWallpaper] || WALLPAPER_TONES.alpine;
    notchWrapper.style.setProperty('--sim-vibe-bg', tone.bg);
    notchWrapper.style.setProperty('--sim-vibe-ink', tone.ink);
    notchWrapper.style.setProperty('--sim-vibe-elevated', tone.elevated);
    notchWrapper.style.setProperty('--sim-vibe-border', tone.border);
    notchWrapper.style.setProperty('--sim-vibe-accent', tone.accent);
  }

  applyAdaptiveTone();

  // Wallpaper Switcher
  const wpSwatches = document.querySelectorAll('.wp-swatch');
  wpSwatches.forEach(swatch => {
    swatch.addEventListener('click', () => {
      wpSwatches.forEach(s => s.classList.remove('active'));
      swatch.classList.add('active');
      currentWallpaper = swatch.dataset.wp;
      if (viewport) {
        viewport.setAttribute('data-wallpaper', currentWallpaper);
      }
      applyAdaptiveTone();
    });
  });

  // Vibe Dot Switcher
  const vibeDots = document.querySelectorAll('.vibe-dot');
  vibeDots.forEach(dot => {
    dot.addEventListener('click', () => {
      vibeDots.forEach(d => {
        d.classList.remove('active');
        d.setAttribute('aria-checked', 'false');
      });
      dot.classList.add('active');
      dot.setAttribute('aria-checked', 'true');
      currentVibe = dot.dataset.vibe;
      if (notchWrapper) {
        notchWrapper.setAttribute('data-vibe', currentVibe);
      }
      if (currentVibe === 'adaptive') {
        applyAdaptiveTone();
      }
    });
  });

  // Magnetic Drag-and-Snap Physics
  if (dragHandle && notchWrapper && viewport) {
    let isDragging = false;
    let pointerStartX = 0;
    let pointerStartY = 0;
    let initialNotchLeft = 0;
    let initialNotchTop = 0;

    dragHandle.addEventListener('pointerdown', (e) => {
      isDragging = true;
      dragHandle.setPointerCapture(e.pointerId);

      const viewportRect = viewport.getBoundingClientRect();
      const notchRect = notchWrapper.getBoundingClientRect();

      pointerStartX = e.clientX;
      pointerStartY = e.clientY;

      initialNotchLeft = notchRect.left - viewportRect.left;
      initialNotchTop = notchRect.top - viewportRect.top;

      notchWrapper.style.transition = 'none';
      notchWrapper.style.left = `${initialNotchLeft}px`;
      notchWrapper.style.right = 'auto';
      notchWrapper.style.top = `${initialNotchTop}px`;
    });

    dragHandle.addEventListener('pointermove', (e) => {
      if (!isDragging) return;

      const viewportRect = viewport.getBoundingClientRect();
      const notchRect = notchWrapper.getBoundingClientRect();

      const dx = e.clientX - pointerStartX;
      const dy = e.clientY - pointerStartY;

      let newLeft = initialNotchLeft + dx;
      let newTop = initialNotchTop + dy;

      // Bound within viewport bounds
      const maxLeft = viewportRect.width - notchRect.width;
      const maxTop = viewportRect.height - notchRect.height - 12;

      newLeft = Math.max(0, Math.min(maxLeft, newLeft));
      newTop = Math.max(30, Math.min(maxTop, newTop));

      notchWrapper.style.left = `${newLeft}px`;
      notchWrapper.style.top = `${newTop}px`;
    });

    const endDrag = (e) => {
      if (!isDragging) return;
      isDragging = false;
      try {
        dragHandle.releasePointerCapture(e.pointerId);
      } catch {}

      const viewportRect = viewport.getBoundingClientRect();
      const notchRect = notchWrapper.getBoundingClientRect();

      const notchCenterX = (notchRect.left - viewportRect.left) + notchRect.width / 2;
      const snapToLeft = notchCenterX < viewportRect.width / 2;

      notchWrapper.style.transition = 'left 0.38s cubic-bezier(0.22, 1, 0.36, 1), right 0.38s cubic-bezier(0.22, 1, 0.36, 1), top 0.38s cubic-bezier(0.22, 1, 0.36, 1)';

      if (snapToLeft) {
        currentEdge = 'left';
        notchWrapper.setAttribute('data-edge', 'left');
        notchWrapper.style.left = '0px';
        notchWrapper.style.right = 'auto';
      } else {
        currentEdge = 'right';
        notchWrapper.setAttribute('data-edge', 'right');
        notchWrapper.style.left = 'auto';
        notchWrapper.style.right = '0px';
      }
    };

    dragHandle.addEventListener('pointerup', endDrag);
    dragHandle.addEventListener('pointercancel', endDrag);

    // Keep notch strictly within canvas on window resize / orientation change
    const resizeObserver = new ResizeObserver(() => {
      if (!isDragging) {
        if (currentEdge === 'left') {
          notchWrapper.style.left = '0px';
          notchWrapper.style.right = 'auto';
        } else {
          notchWrapper.style.left = 'auto';
          notchWrapper.style.right = '0px';
        }
      }
    });
    resizeObserver.observe(viewport);
  }

  // ── 4. Interactive Sample Compression Demo State Machine ─────────────────

  const SAMPLE_DATA = {
    pdf: {
      name: 'Resume_Portfolio.pdf',
      oldBytes: 1677721,
      oldFormatted: '1.60 MB',
      targets: {
        200: { newBytes: 194560, newFormatted: '190 KB', pct: '-88%', note: 'Aggressive image compression. Text streams preserved razor-sharp.' },
        500: { newBytes: 479232, newFormatted: '468 KB', pct: '-71%', note: 'All text vector sharp. Image assets downsampled to 220 DPI.' },
        1000: { newBytes: 880640, newFormatted: '860 KB', pct: '-46%', note: 'High fidelity print profile with lossless font subsetting.' }
      }
    },
    png: {
      name: 'Retina_Screenshot.png',
      oldBytes: 4404019,
      oldFormatted: '4.20 MB',
      targets: {
        200: { newBytes: 198656, newFormatted: '194 KB', pct: '-95%', note: 'Indexed palette with 128 colors. Crisp UI text outlines.' },
        500: { newBytes: 501760, newFormatted: '490 KB', pct: '-88%', note: 'Palette quantized to 256 colors with perceptual dithering.' },
        1000: { newBytes: 942080, newFormatted: '920 KB', pct: '-78%', note: 'Truecolor WebP export with lossless alpha channel.' }
      }
    },
    pptx: {
      name: 'Project_Deck.pptx',
      oldBytes: 7130316,
      oldFormatted: '6.80 MB',
      targets: {
        200: { newBytes: 198656, newFormatted: '194 KB', pct: '-97%', note: 'Slide images compressed to 150 DPI. Presentation XML intact.' },
        500: { newBytes: 491520, newFormatted: '480 KB', pct: '-93%', note: 'Embedded bitmaps recompressed without breaking layout.' },
        1000: { newBytes: 1003520, newFormatted: '980 KB', pct: '-86%', note: 'Embedded graphics optimized. Slides and vector shapes untouched.' }
      }
    }
  };

  let activeSample = 'pdf';
  let activeTargetSize = 500;

  const sampleChips = document.querySelectorAll('.sample-chip');
  const sizePills = document.querySelectorAll('.size-pill');
  const targetSizeDisplay = document.getElementById('target-size-display');
  const compressBtn = document.getElementById('sim-compress-btn');
  const resetBtn = document.getElementById('sim-reset-btn');

  const viewConfig = document.getElementById('notch-view-config');
  const viewProgress = document.getElementById('notch-view-progress');
  const viewResult = document.getElementById('notch-view-result');

  const progressTaskLabel = document.getElementById('progress-task-label');
  const progressPctDisplay = document.getElementById('progress-pct-display');
  const progressFillBar = document.getElementById('progress-fill-bar');

  const resOldDisplay = document.getElementById('res-old-display');
  const resNewDisplay = document.getElementById('res-new-display');
  const resPctBadge = document.getElementById('res-pct-badge');
  const resClarityNote = document.getElementById('res-clarity-note');
  const dragPillLabel = document.getElementById('drag-pill-label');

  // Sample file selection
  sampleChips.forEach(chip => {
    chip.addEventListener('click', () => {
      sampleChips.forEach(c => {
        c.classList.remove('active');
        c.setAttribute('aria-checked', 'false');
      });
      chip.classList.add('active');
      chip.setAttribute('aria-checked', 'true');
      activeSample = chip.dataset.sample;
    });
  });

  // Target size pill selection
  sizePills.forEach(pill => {
    pill.addEventListener('click', () => {
      sizePills.forEach(p => {
        p.classList.remove('active');
        p.setAttribute('aria-checked', 'false');
      });
      pill.classList.add('active');
      pill.setAttribute('aria-checked', 'true');
      activeTargetSize = parseInt(pill.dataset.size, 10);
      if (targetSizeDisplay) {
        targetSizeDisplay.textContent = activeTargetSize === 1000 ? '1 MB' : `${activeTargetSize} KB`;
      }
    });
  });

  // Compress button action
  if (compressBtn) {
    compressBtn.addEventListener('click', () => {
      if (!viewConfig || !viewProgress || !viewResult) return;

      viewConfig.classList.remove('active');
      viewProgress.classList.add('active');

      const sample = SAMPLE_DATA[activeSample];
      if (progressTaskLabel) {
        progressTaskLabel.textContent = activeSample === 'pdf'
          ? 'Optimizing PDF streams...'
          : activeSample === 'png'
            ? 'Quantizing color palette...'
            : 'Recompressing slide media...';
      }

      // Smooth 450ms progress animation
      let progress = 0;
      const startTime = performance.now();
      const duration = 450;

      function updateProgress(now) {
        const elapsed = now - startTime;
        progress = Math.min(100, Math.round((elapsed / duration) * 100));

        if (progressFillBar) progressFillBar.style.width = `${progress}%`;
        if (progressPctDisplay) progressPctDisplay.textContent = `${progress}%`;

        if (progress < 100) {
          requestAnimationFrame(updateProgress);
        } else {
          // Complete and show result
          setTimeout(() => {
            viewProgress.classList.remove('active');
            viewResult.classList.add('active');

            const resData = sample.targets[activeTargetSize];
            if (resOldDisplay) resOldDisplay.textContent = sample.oldFormatted;
            if (resNewDisplay) resNewDisplay.textContent = resData.newFormatted;
            if (resPctBadge) resPctBadge.textContent = resData.pct;
            if (resClarityNote) resClarityNote.textContent = resData.note;
            if (dragPillLabel) dragPillLabel.textContent = sample.name;
          }, 80);
        }
      }

      requestAnimationFrame(updateProgress);
    });
  }

  // Reset demo
  if (resetBtn) {
    resetBtn.addEventListener('click', () => {
      if (!viewConfig || !viewResult) return;
      viewResult.classList.remove('active');
      viewConfig.classList.add('active');
      if (progressFillBar) progressFillBar.style.width = '0%';
    });
  }

  // ── 5. Engine & Format Specs Segmented Tab Switcher ──────────────────────

  const SPEC_DATA = {
    photos: {
      title: 'Photos & Raster Graphics',
      prose: 'Combines high-precision binary search quality tuning for JPEG and WebP with indexed palette quantization for PNG. Inspects the visual entropy of images to stop compression before blocking artifacts appear. Automatically scrubs GPS location coordinates and camera serial numbers upon export.',
      formats: 'JPG, PNG, WebP, HEIC, TIFF, BMP',
      engine: 'OpenCV 4.8 + lodepng (Pure C++20)',
      guarantee: 'Metadata stripped · Decompression bomb capped',
      savings: '40% – 85%'
    },
    pdf: {
      title: 'PDF Documents & Scans',
      prose: 'Restructures internal PDF cross-reference tables and compresses object streams using libqpdf. High-resolution raster images embedded inside documents are downsampled to target DPI in a single pass while keeping selectable text, vector lines, and digital signatures crisp and intact.',
      formats: 'PDF 1.2 – 2.0, PDF/A, Scanned Documents',
      engine: 'libqpdf 11 + Ghostscript (-dSAFER)',
      guarantee: 'Searchable text preserved · Single-pass processing',
      savings: '50% – 80%'
    },
    office: {
      title: 'Office Open XML Documents',
      prose: 'Treats DOCX, PPTX, and XLSX files as structured zip archives. Extracts and optimizes embedded media assets without modifying XML formatting or layout definitions. Incorporates hardened Zip Slip and Zip Bomb guards with a strict 100:1 inflation ceiling.',
      formats: 'DOCX, PPTX, XLSX',
      engine: 'Custom C++20 Deflate Stream Recompressor',
      guarantee: 'Zip Slip blocked · XML schemas untouched',
      savings: '45% – 75%'
    },
    media: {
      title: 'Video & Audio Clips',
      prose: 'Re-encodes video using rate-controlled H.264 or VP9 with audio bitrate normalization. Subprocess execution runs in a sandboxed environment with strict protocol whitelisting (file, pipe only) and watchdog timers that kill runaway encoding jobs.',
      formats: 'MP4, MOV, MKV, WEBM, MP3',
      engine: 'FFmpeg / FFprobe (Sandboxed argv invocation)',
      guarantee: 'Sandbox restricted · Watchdog timeouts',
      savings: '35% – 70%'
    }
  };

  const specTabs = document.querySelectorAll('.spec-tab');
  const specTitle = document.getElementById('spec-title');
  const specProse = document.getElementById('spec-prose');
  const specFormats = document.getElementById('spec-formats');
  const specEngine = document.getElementById('spec-engine');
  const specGuarantee = document.getElementById('spec-guarantee');
  const specSavings = document.getElementById('spec-savings');

  specTabs.forEach(tab => {
    tab.addEventListener('click', () => {
      specTabs.forEach(t => {
        t.classList.remove('active');
        t.setAttribute('aria-selected', 'false');
      });
      tab.classList.add('active');
      tab.setAttribute('aria-selected', 'true');

      const data = SPEC_DATA[tab.dataset.spec];
      if (!data) return;

      if (specTitle) specTitle.textContent = data.title;
      if (specProse) specProse.textContent = data.prose;
      if (specFormats) specFormats.textContent = data.formats;
      if (specEngine) specEngine.textContent = data.engine;
      if (specGuarantee) specGuarantee.textContent = data.guarantee;
      if (specSavings) specSavings.textContent = data.savings;
    });
  });

  // ── 6. First-Launch Notice OS Tab Switcher ────────────────────────────────

  const noticeTabs = document.querySelectorAll('.notice-tab');
  const noticePanels = {
    mac: document.getElementById('notice-panel-mac'),
    win: document.getElementById('notice-panel-win'),
    linux: document.getElementById('notice-panel-linux')
  };

  noticeTabs.forEach(tab => {
    tab.addEventListener('click', () => {
      noticeTabs.forEach(t => {
        t.classList.remove('active');
        t.setAttribute('aria-selected', 'false');
      });
      tab.classList.add('active');
      tab.setAttribute('aria-selected', 'true');

      Object.keys(noticePanels).forEach(os => {
        if (noticePanels[os]) {
          noticePanels[os].classList.toggle('active', os === tab.dataset.os);
        }
      });
    });
  });

  // Automatically select the user's detected OS in the first-launch guide
  const detectedOS = detectClientOS();
  if (detectedOS === 'win' || detectedOS === 'linux') {
    const targetTab = document.querySelector(`.notice-tab[data-os="${detectedOS}"]`);
    if (targetTab) targetTab.click();
  }

  // ── 7. Checksum Command Copy Button ──────────────────────────────────────

  const copyChecksumBtn = document.getElementById('copy-checksum-btn');
  const copyBtnText = document.getElementById('copy-btn-text');

  if (copyChecksumBtn && checksumCommandCode) {
    copyChecksumBtn.addEventListener('click', async () => {
      try {
        await navigator.clipboard.writeText(checksumCommandCode.textContent.trim());
        if (copyBtnText) copyBtnText.textContent = 'Copied';
        setTimeout(() => {
          if (copyBtnText) copyBtnText.textContent = 'Copy';
        }, 2000);
      } catch {
        if (copyBtnText) copyBtnText.textContent = 'Failed';
      }
    });
  }

  // ── 8. Simulated Clock in Desktop Menubar ─────────────────────────────────

  const clockEl = document.getElementById('sim-clock');
  function updateSimClock() {
    if (!clockEl) return;
    const now = new Date();
    let hours = now.getHours();
    const minutes = String(now.getMinutes()).padStart(2, '0');
    const ampm = hours >= 12 ? 'PM' : 'AM';
    hours = hours % 12 || 12;
    clockEl.textContent = `${hours}:${minutes} ${ampm}`;
  }
  updateSimClock();
  setInterval(updateSimClock, 30000);

})();
