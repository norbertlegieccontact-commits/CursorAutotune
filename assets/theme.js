/* ============================================================
   HEADER — mobile nav toggle
   ============================================================ */
(function () {
  const toggleBtn = document.querySelector('.header__menu-toggle');
  const mobileNav = document.getElementById('mobile-nav');
  if (toggleBtn && mobileNav) {
    toggleBtn.addEventListener('click', function () {
      const expanded = this.getAttribute('aria-expanded') === 'true';
      this.setAttribute('aria-expanded', String(!expanded));
      if (expanded) {
        mobileNav.setAttribute('hidden', '');
      } else {
        mobileNav.removeAttribute('hidden');
      }
    });
  }
})();

/* ============================================================
   PRODUCT GALLERY — thumbnail switching
   ============================================================ */
(function () {
  const featuredImg = document.getElementById('product-featured-image');
  const thumbs = document.querySelectorAll('.product-gallery__thumb');
  if (!featuredImg || !thumbs.length) return;

  thumbs.forEach(function (thumb) {
    thumb.addEventListener('click', function () {
      const thumbImg = this.querySelector('img');
      if (!thumbImg) return;
      featuredImg.src = thumbImg.src.replace('_120x120', '_900x');
      featuredImg.srcset = '';
      thumbs.forEach(function (t) { t.classList.remove('is-active'); });
      this.classList.add('is-active');
    });
  });
})();

/* ============================================================
   QUANTITY SELECTOR
   ============================================================ */
(function () {
  document.addEventListener('click', function (e) {
    const btn = e.target.closest('.quantity-selector__btn');
    if (!btn) return;
    const selector = btn.closest('.quantity-selector');
    const input = selector ? selector.querySelector('.quantity-selector__input') : null;
    if (!input) return;
    const current = parseInt(input.value, 10) || 1;
    const action = btn.getAttribute('data-action');
    if (action === 'decrease' || btn.textContent.trim() === '-') {
      input.value = Math.max(0, current - 1);
    } else {
      input.value = current + 1;
    }
    input.dispatchEvent(new Event('change', { bubbles: true }));
  });
})();

/* ============================================================
   VARIANT SELECTION — update hidden input
   ============================================================ */
(function () {
  const productForm = document.getElementById('product-form');
  if (!productForm) return;

  const variantIdInput = productForm.querySelector('input[name="id"]');
  const optionInputs = productForm.querySelectorAll('.product-option__radio');

  if (!variantIdInput || !optionInputs.length) return;

  const variantData = window.__variantData || [];

  optionInputs.forEach(function (radio) {
    radio.addEventListener('change', function () {
      const selectedOptions = {};
      productForm.querySelectorAll('.product-option__radio:checked').forEach(function (r) {
        selectedOptions[r.name] = r.value;
      });

      const matched = variantData.find(function (v) {
        return v.options.every(function (opt, i) {
          return opt === selectedOptions['option' + (i + 1)] || opt === Object.values(selectedOptions)[i];
        });
      });

      if (matched) {
        variantIdInput.value = matched.id;
        const addBtn = productForm.querySelector('.product-page__atc');
        if (addBtn) {
          addBtn.disabled = !matched.available;
          addBtn.textContent = matched.available ? 'Add to cart' : 'Sold out';
        }
      }
    });
  });
})();

/* ============================================================
   CART FORM — quantity update on change
   ============================================================ */
(function () {
  const cartForm = document.getElementById('cart-form');
  if (!cartForm) return;
  let debounceTimer;
  cartForm.addEventListener('change', function (e) {
    if (!e.target.matches('input[name="updates[]"]')) return;
    clearTimeout(debounceTimer);
    debounceTimer = setTimeout(function () {
      cartForm.submit();
    }, 800);
  });
})();

/* ============================================================
   SCROLL ANIMATIONS — Intersection Observer
   ============================================================ */
(function () {
  if (!window.IntersectionObserver) return;
  if (!document.body.classList.contains('animations-enabled')) return;

  const animatedEls = document.querySelectorAll(
    '.product-card, .multicolumn__item, .testimonials__item, .section-header, .image-with-text__inner > *'
  );

  const observer = new IntersectionObserver(function (entries) {
    entries.forEach(function (entry) {
      if (entry.isIntersecting) {
        entry.target.style.animationPlayState = 'running';
        observer.unobserve(entry.target);
      }
    });
  }, { threshold: 0.1 });

  animatedEls.forEach(function (el) {
    el.style.animationPlayState = 'paused';
    observer.observe(el);
  });
})();
