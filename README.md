# VoxTune — Shopify Theme

Motyw (theme) Shopify Online Store 2.0, który odwzorowuje **1:1** interfejs VoxTune
(z `Assets/index.html`) jako stronę główną sklepu. Wszystkie teksty, kolory,
presety i etykiety knobów są edytowalne w panelu **Sklep online → Motywy → Dostosuj**.

## Podgląd

Otwórz `docs/preview.html` w przeglądarce, aby zobaczyć stronę główną dokładnie
tak, jak będzie wyglądać w Shopify (z domyślnymi ustawieniami).

## Struktura motywu

```
assets/voxtune.js              # logika UI (knoby, presety, wizualizer)
config/settings_schema.json    # globalne ustawienia motywu
config/settings_data.json      # zapisane wartości ustawień
layout/theme.liquid            # szkielet HTML, fonty, <head>
locales/en.default.json        # tłumaczenia
sections/voxtune-plugin.liquid # sekcja z designem VoxTune + edytowalny schemat
templates/index.json           # strona główna (sekcja VoxTune + 13 presetów)
templates/*.liquid             # pozostałe wymagane szablony Shopify
```

## Wdrożenie do Shopify

### Wariant A — Shopify CLI (zalecany)

1. Zainstaluj Shopify CLI: `npm install -g @shopify/cli`
2. W katalogu repozytorium: `shopify theme dev --store twoj-sklep.myshopify.com`
   (podgląd na żywo) lub `shopify theme push` (wgranie do sklepu).
3. W panelu Shopify: **Sklep online → Motywy → Dostosuj**, aby edytować teksty,
   kolory i presety.

### Wariant B — wgranie pliku ZIP

1. Spakuj zawartość katalogów `assets, config, layout, locales, sections, snippets,
   templates` do pliku `.zip` (foldery muszą być w korzeniu archiwum).
2. Shopify: **Sklep online → Motywy → Dodaj motyw → Prześlij plik ZIP**.

## Co można edytować w „Dostosuj”

W sekcji **VoxTune Plugin** (strona główna):

- **Brand / Logo** — teksty logo (VOX / TUNE) oraz URL w stopce.
- **Header buttons** — etykiety przycisków Bypass / Formant / Low Latency.
- **Labels** — Pitch Detection, Key, Scale, AI Auto Key, tytuł sekcji formant.
- **Knobs** — etykiety i wartości domyślne trzech głównych knobów.
- **Colors** — akcenty (cyan / magenta / purple), kolory tekstu i tło strony.
- **Layout** — szerokość pluginu, pokazywanie podpowiedzi klawiszowych.
- **Presety** — bloki „Preset” (kategoria, nazwa, Speed / Humanize / Mix).
  Można dodawać, usuwać i sortować presety; lista i wartości aktualizują się
  automatycznie w UI.

## Walidacja

Motyw przechodzi `shopify theme check` bez błędów (`.theme-check.yml` wyłącza
jedynie ostrzeżenie `RemoteAsset` dla fontów Google Fonts używanych dla wierności
designu 1:1).
