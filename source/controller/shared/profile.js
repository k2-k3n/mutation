// Mutation: what the controllers show and the CC each control sends (channel 1).
// src/app.cpp binds the same numbers in kBind.
const pct = v => `${v}%`;
export const profile = {
  name: 'Mutation',
  accent: '#6b3f86',
  pages: [
    {key: 'P', label: 'PLAY', title: 'Play the strain', color: '#9C7BC4', controls: [
      {id: 'parent', cc: 30, kind: 'trigger', label: 'A', key: 'A'},
      {id: 'mutant', cc: 31, kind: 'trigger', label: 'B', key: 'B'},
      {id: 'smear', cc: 32, kind: 'trigger', label: 'SMEAR', key: 'S'},
      {id: 'dryHold', cc: 33, kind: 'trigger', label: 'DRY', key: 'D'},
      {id: 'space', kind: 'xy', label: 'Blend × Drive', x: 'blend', y: 'drive'},
    ]},
    {key: 'B', label: 'BREED', title: 'Breed', color: '#9BBF4A', controls: [
      {id: 'blend', cc: 20, kind: 'range', label: 'Blend', min: 0, max: 100, def: 72, format: pct},
      {id: 'strain', cc: 28, kind: 'readout', label: 'Strain', min: 0, max: 99, def: 4, format: v => String(v).padStart(2, '0')},
      {id: 'new', cc: 34, kind: 'trigger', label: 'NEW', key: 'N'},
      {id: 'keep', cc: 35, kind: 'trigger', label: 'KEEP', key: 'K'},
      {id: 'back', cc: 36, kind: 'trigger', label: 'BACK', key: 'B'},
    ]},
    {key: 'M', label: 'MUTANT', title: 'The mutant', color: '#E3A07A', controls: [
      {id: 'drive', cc: 23, kind: 'range', label: 'Drive', min: 0, max: 100, def: 75},
      {id: 'tone', cc: 24, kind: 'range', label: 'Tone', min: 0, max: 100, def: 32},
      {id: 'mod', cc: 25, kind: 'range', label: 'Mod', min: 0, max: 100, def: 70},
      {id: 'echo', cc: 26, kind: 'range', label: 'Echo', min: 0, max: 100, def: 62},
    ]},
    {key: 'F', label: 'FAVES', title: 'Favourites', color: '#D4BC5C', controls: [
      {id: 'fav', cc: 22, kind: 'choice', label: 'Favourite', options: ['–', '1', '2', '3', '4', '5', '6', '7', '8'], def: 0},
      {id: 'kept', cc: 29, kind: 'readout', label: 'Kept', min: 0, max: 8, def: 0, format: v => `${v} / 8`},
      {id: 'dry', cc: 21, kind: 'toggle', label: 'Dry', def: 0},
      {id: 'level', cc: 27, kind: 'range', label: 'Level', min: 0, max: 100, def: 84, format: pct},
    ]},
  ],
};
