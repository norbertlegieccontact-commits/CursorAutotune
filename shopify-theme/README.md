# VoxTune — motyw Shopify

Motyw Online Store 2.0 odwzorowujący 1:1 interaktywną stronę VoxTune z projektu Claude Code (`Assets/index.html`).

## Co zawiera motyw

- Pełny interfejs pluginu VoxTune (knoby, waveform, presety, formant panel)
- Interaktywna demonstracja (bez połączenia z pluginem JUCE — działa jako demo na sklepie)
- Edycja w Shopify Theme Editor:
  - Logo (VOX / TUNE)
  - URL marki
  - Tytuł panelu formant
  - Domyślny preset
  - Kolory akcentów (cyan, magenta, purple)
  - Tło strony
  - Opcjonalny przycisk „Kup”
  - Opcjonalna lista presetów (bloki)

## Wdrożenie — metoda 1: Shopify CLI (zalecana)

### Wymagania

- Konto Shopify (plan Basic lub wyższy)
- [Shopify CLI](https://shopify.dev/docs/api/shopify-cli)

### Kroki

```bash
# 1. Zainstaluj Shopify CLI (jeśli nie masz)
npm install -g @shopify/cli @shopify/theme

# 2. Przejdź do folderu motywu
cd shopify-theme

# 3. Zaloguj się i połącz ze sklepem
shopify theme dev --store TWOJ-SKLEP.myshopify.com

# 4. Po weryfikacji wyglądu — opublikuj motyw
shopify theme push --store TWOJ-SKLEP.myshopify.com
```

`shopify theme dev` uruchamia podgląd na żywo z hot-reload. Po zatwierdzeniu użyj `shopify theme push`, aby wgrać motyw na sklep.

## Wdrożenie — metoda 2: Upload ZIP (bez CLI)

1. Spakuj **zawartość** folderu `shopify-theme/` (nie sam folder nadrzędny) do pliku ZIP.
2. W panelu Shopify: **Online Store → Themes → Add theme → Upload zip file**.
3. Po wgraniu kliknij **Customize** na nowym motywie.
4. Ustaw motyw jako **Publish** (Opublikuj).

## Konfiguracja strony głównej

Motyw ma domyślnie szablon `index.json` z sekcją **VoxTune Showcase**. Po publikacji strona główna sklepu wyświetli interfejs pluginu.

W edytorze motywu (**Customize**) możesz zmieniać:

| Ustawienie | Opis |
|---|---|
| Logo line 1 / 2 | Tekst logo (domyślnie VOX / TUNE) |
| Brand URL | Tekst w stopce (np. vox-tune.com) |
| Formant panel title | Nagłówek sekcji formant (VoxShift Pro) |
| Colors | Kolory akcentów i tła |
| Show buy button | Przycisk CTA pod pluginem |
| Preset blocks | Własna lista presetów (opcjonalnie) |

## Sprzedaż produktu cyfrowego (plugin)

1. W Shopify: **Products → Add product** — dodaj VoxTune jako produkt cyfrowy.
2. W Theme Editor włącz **Show buy button** i ustaw link do produktu.
3. Dla plików cyfrowych użyj aplikacji Shopify do digital downloads lub linku do pobrania w opisie produktu.

## Struktura plików

```
shopify-theme/
├── assets/
│   ├── voxtune.css      # Style 1:1 z oryginału
│   └── voxtune.js       # Interakcje (demo mode)
├── config/
│   └── settings_schema.json
├── layout/
│   └── theme.liquid
├── locales/
├── sections/
│   └── voxtune-showcase.liquid   # Główna sekcja
└── templates/
    └── index.json       # Strona główna
```

## Uwagi techniczne

- Oryginalny HTML łączył się z pluginem JUCE przez `window.__JUCE__`. Na Shopify działa tryb demo — wszystkie kontrolki są interaktywne, ale nie sterują prawdziwym pluginem audio.
- Szerokość pluginu: 700px (jak w oryginale).
- Obsługa dotyku na urządzeniach mobilnych (knoby reagują na swipe).

## Źródło designu

Oryginalny plik: `Assets/index.html` na gałęzi `cursor/add-autotune-project-rules-e22f`.
