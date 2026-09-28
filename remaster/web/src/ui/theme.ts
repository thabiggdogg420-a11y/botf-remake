// Faction identity: accent palettes, emblem glyphs (non-colour owner cue) and flavour copy.
// Emblems are original geometric marks, not reproductions of any studio artwork.

export interface Palette {
  key: string;
  primary: string;     // main accent (LCARS frame, buttons)
  secondary: string;   // second accent
  tertiary: string;    // muted accent
  glow: string;        // rgba glow
  map: string;         // owner colour on the galaxy map
  ink: string;         // text colour on primary fills
}

export const PALETTES: Record<number, Palette> = {
  0: { key: 'neutral', primary: '#8fa6c8', secondary: '#c3cfe0', tertiary: '#5b6b86', glow: 'rgba(143,166,200,0.35)', map: '#9aa6b8', ink: '#05070d' },
  1: { key: 'federation', primary: '#6fa8ff', secondary: '#f2b45a', tertiary: '#a8b8ff', glow: 'rgba(111,168,255,0.38)', map: '#5b9dff', ink: '#050912' },
  2: { key: 'klingon', primary: '#e0484a', secondary: '#c98a4b', tertiary: '#9e3a36', glow: 'rgba(224,72,74,0.38)', map: '#e2463f', ink: '#120505' },
  3: { key: 'romulan', primary: '#3fc98a', secondary: '#93dcbf', tertiary: '#2b8c62', glow: 'rgba(63,201,138,0.36)', map: '#35c37f', ink: '#03110a' },
  4: { key: 'cardassian', primary: '#d9a44a', secondary: '#e9d6a6', tertiary: '#9c7437', glow: 'rgba(217,164,74,0.36)', map: '#d6a13f', ink: '#120c03' },
  5: { key: 'ferengi', primary: '#f08a2c', secondary: '#c8743d', tertiary: '#f5c16c', glow: 'rgba(240,138,44,0.38)', map: '#f28c28', ink: '#130902' },
};

// Colour-blind-safe owner colours (Okabe–Ito): sky blue, vermillion, bluish green, yellow,
// reddish purple. Distinguishable under protanopia, deuteranopia and tritanopia; emblem
// silhouettes remain the primary, colour-independent owner cue.
export const COLORBLIND_PALETTES: Record<number, Palette> = {
  0: PALETTES[0]!,
  1: { key: 'federation', primary: '#56b4e9', secondary: '#e69f00', tertiary: '#9fd3f2', glow: 'rgba(86,180,233,0.38)', map: '#56b4e9', ink: '#03101a' },
  2: { key: 'klingon', primary: '#e0662a', secondary: '#f0b27a', tertiary: '#b3521f', glow: 'rgba(213,94,0,0.4)', map: '#d55e00', ink: '#140700' },
  3: { key: 'romulan', primary: '#16b38a', secondary: '#8fe0c8', tertiary: '#0b7a5e', glow: 'rgba(0,158,115,0.38)', map: '#009e73', ink: '#00130d' },
  4: { key: 'cardassian', primary: '#f0e442', secondary: '#faf3a8', tertiary: '#b8ad2a', glow: 'rgba(240,228,66,0.34)', map: '#f0e442', ink: '#141200' },
  5: { key: 'ferengi', primary: '#d98cbb', secondary: '#f0c6df', tertiary: '#a8628e', glow: 'rgba(204,121,167,0.38)', map: '#cc79a7', ink: '#14060e' },
};

let colorblind = false;
/** Switches owner colours between the standard and colour-blind-safe palettes. */
export function setColorblindPalette(on: boolean): void { colorblind = on; }
export function colorblindPaletteActive(): boolean { return colorblind; }

export function palette(empire: number): Palette {
  const table = colorblind ? COLORBLIND_PALETTES : PALETTES;
  return table[empire] ?? table[0]!;
}

export interface FactionInfo {
  id: number;
  short: string;
  motto: string;
  traits: string[];
  capital: string;
}

export const FACTIONS: FactionInfo[] = [
  { id: 1, short: 'Federation', capital: 'Sol', motto: 'Explore. Unite. Endure.', traits: ['Balanced economy', 'Diplomatic reach', 'Science vessels'] },
  { id: 2, short: 'Klingon', capital: "Qo'noS", motto: 'Glory is earned in fire.', traits: ['Aggressive fleets', 'Cloaking raiders', 'Warrior culture'] },
  { id: 3, short: 'Romulan', capital: 'Romulus', motto: 'Patience is the sharpest blade.', traits: ['Cloaked warbirds', 'Espionage', 'Secretive state'] },
  { id: 4, short: 'Cardassian', capital: 'Cardassia', motto: 'Order. Duty. The State.', traits: ['Industrial might', 'Security services', 'Disciplined'] },
  { id: 5, short: 'Ferengi', capital: 'Ferenginar', motto: 'Every contact is a customer.', traits: ['Trade & credits', 'Opportunists', 'Fast traders'] },
];

// Emblem paths in a 0..100 box. Distinct silhouettes so owners read without colour.
export const EMBLEMS: Record<number, string> = {
  0: 'M50 12 L88 50 L50 88 L12 50 Z',
  // Federation: arrowhead chevron within an ellipse cut.
  1: 'M50 8 L78 86 L50 68 L22 86 Z',
  // Klingon: three-bladed trefoil.
  2: 'M50 6 L60 42 L94 58 L58 60 L50 94 L42 60 L6 58 L40 42 Z',
  // Romulan: raptor wings.
  3: 'M50 20 L62 44 L96 30 L72 62 L56 60 L50 84 L44 60 L28 62 L4 30 L38 44 Z',
  // Cardassian: tiered inverted pyramid.
  4: 'M10 16 L90 16 L78 38 L70 38 L50 90 L30 38 L22 38 Z',
  // Ferengi: stacked coin / latinum ring.
  5: 'M50 8 C74 8 92 26 92 50 C92 74 74 92 50 92 C26 92 8 74 8 50 C8 26 26 8 50 8 Z M50 30 C39 30 30 39 30 50 C30 61 39 70 50 70 C61 70 70 61 70 50 C70 39 61 30 50 30 Z',
};

export const CATEGORY_LABELS: Record<string, string> = {
  construction: 'Construction',
  research: 'Research',
  exploration: 'Exploration',
  diplomacy: 'Diplomacy',
  intel: 'Intelligence',
  combat: 'Combat',
  colonization: 'Colonization',
  events: 'Galactic events',
  stations: 'Stations',
  victory: 'Victory',
  economy: 'Colonies & economy',
  fleets: 'Fleet operations',
};

export const RESEARCH_BLURBS = [
  'Life sciences, terraforming and food production.',
  'Computing, sensors, research output and intelligence.',
  'Engineering, industry and hull construction.',
  'Power generation, shields and beam weapons.',
  'Warp drive, speed and operational range.',
  'Weapon systems, torpedoes and ground assault.',
];
