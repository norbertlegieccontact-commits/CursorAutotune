# CursorAutotune Shopify Theme

Minimalny Shopify Online Store 2.0 theme przygotowany pod migracje strony
zaprojektowanej w Claude Code na Shopify.

## Co jest w repo

- `layout/theme.liquid` - glowny layout Shopify.
- `templates/index.json` - strona glowna z sekcja landing page.
- `sections/one-to-one-page.liquid` - edytowalna sekcja do odwzorowania strony 1:1.
- `sections/main-page.liquid` - bazowy template zwyklej strony Shopify.
- `assets/theme.css` - style bazowe, gotowe do zastapienia lub rozszerzenia CSS z projektu.
- `assets/theme.js` - miejsce na JavaScript strony.
- `config/settings_schema.json` - ustawienia theme w Shopify Theme Editor.

## Jak przeniesc projekt 1:1 z Claude Code

Do pelnego odwzorowania potrzebne sa oryginalne pliki projektu: HTML/JSX,
CSS/Tailwind, obrazy, fonty i inne assety. W tym repo nie bylo jeszcze tych
plikow, dlatego obecny theme jest gotowym szkieletem migracyjnym.

1. Skopiuj obrazy, fonty i inne statyczne assety do `assets/`.
2. Przenies globalne style z projektu do `assets/theme.css`.
3. Przenies logike JS do `assets/theme.js`, usuwajac rzeczy specyficzne dla
   React/Vite/Next, jesli byly uzywane tylko do renderowania.
4. Odtworz markup strony w `sections/one-to-one-page.liquid`.
5. Elementy, ktore maja byc edytowalne w Shopify, podlacz do `settings` lub
   `blocks` w schema tej sekcji.
6. Dla elementow, ktore maja pozostac absolutnie 1:1, uzyj ustawien
   `Custom Liquid or HTML` oraz `Custom CSS`, albo wklej je bezposrednio do
   sekcji po migracji.

Najlepsza praktyka: najpierw odwzorowac strone statycznie 1:1, a dopiero potem
zamieniac teksty, obrazy, linki i powtarzalne karty na edytowalne pola Shopify.
Dzieki temu latwiej sprawdzic, czy wyglad nie zmienil sie podczas migracji.

## Uruchomienie lokalnie

Wymagania:

- Shopify CLI
- Dostep do sklepu Shopify

Komendy:

```bash
shopify login --store twoj-sklep.myshopify.com
shopify theme dev --store twoj-sklep.myshopify.com
```

Shopify CLI uruchomi preview theme i pozwoli testowac zmiany lokalnie.

## Wdrozenie do Shopify

Najbezpieczniej wyslac motyw jako nieopublikowany:

```bash
shopify theme push --unpublished --store twoj-sklep.myshopify.com
```

Potem w Shopify Admin:

1. Wejdz w `Online Store > Themes`.
2. Otworz wyslany theme w `Customize`.
3. Podmien teksty, linki i obrazy w sekcji `1:1 landing page`.
4. Sprawdz desktop, tablet i mobile.
5. Opublikuj dopiero po porownaniu z oryginalnym projektem.

## Co trzeba doslac, zeby zrobic faktyczne 1:1

Zeby dopracowac finalna wersje bez zgadywania, dodaj do repo albo przeslij:

- kod strony z Claude Code,
- pliki CSS/Tailwind,
- wszystkie obrazy i fonty,
- screenshot lub link do dzialajacego preview,
- informacje, ktore elementy maja byc edytowalne w Shopify.

Po dodaniu tych plikow mozna zamienic obecny starter na dokladny Shopify theme
zgodny z projektem piksel w piksel.
