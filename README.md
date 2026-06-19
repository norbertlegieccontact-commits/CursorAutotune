# CursorAutotune Shopify Theme

This repository now contains a Shopify Online Store 2.0 theme scaffold that can be used to move a designed landing page into Shopify as an editable theme.

## What is included

- Shopify theme structure: `layout`, `templates`, `sections`, `assets`, `config`, and `locales`.
- Editable homepage sections:
  - announcement bar
  - header
  - hero
  - feature grid
  - image with text
  - featured collection
  - testimonials
  - FAQ
  - contact CTA
  - footer
- Core commerce templates:
  - product page
  - collection page
  - cart
  - generic page
  - 404
- Global theme settings for logo, favicon, page width, heading scale, and color tokens.

## Important note about 1:1 matching

The original Claude Code website files are not present in this repository. Because of that, the current theme is a migration-ready Shopify base, not a verified pixel-perfect 1:1 copy of the missing design.

To make it truly 1:1, add the original project files or export to this repo, for example:

- HTML/CSS/JS export
- React/Vite/Next source
- screenshots for desktop/tablet/mobile
- all images, icons, fonts, and brand assets

Then map each original area into the existing Shopify sections, or add custom sections where the design needs exact markup.

## Local development

Install or use Shopify CLI, then authenticate with your store:

```bash
shopify login --store your-store.myshopify.com
shopify theme dev --store your-store.myshopify.com
```

## Upload as an unpublished theme

```bash
shopify theme push --unpublished --store your-store.myshopify.com
```

After upload:

1. Open Shopify Admin.
2. Go to **Online Store -> Themes**.
3. Open **Customize** on the uploaded theme.
4. Replace placeholder copy/images with the final design content.
5. Choose the featured collection for the product section.
6. Preview desktop and mobile before publishing.

## Recommended 1:1 migration workflow

1. Put the original Claude Code project in this repository.
2. Identify every visual section of the original page.
3. Match each section to a Shopify section file in `sections/`.
4. Move static text/images into section schema settings so they stay editable in Shopify.
5. Move CSS into `assets/theme.css`, preserving original spacing, typography, colors, and responsive breakpoints.
6. Test in Shopify Theme Editor.
7. Push the theme as unpublished, preview, then publish only after visual QA.
