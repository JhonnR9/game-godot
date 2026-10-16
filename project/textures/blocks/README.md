# Texturas base dos blocos

Texturas repetíveis 64×64 geradas para a direção de arte inicial do MVP:

| Arquivo | Material |
|---|---|
| `grass.png` | Grama |
| `dirt.png` | Terra |
| `stone.png` | Pedra |
| `sand.png` | Areia |
| `snow.png` | Neve |
| `wood.png` | Madeira |
| `leaves.png` | Folhas |
| `bedrock.png` | Rocha de fundo |

As imagens vieram de uma folha 4×2 de padrões pixelados, separada em tiles quadrados. Cada bloco tem um `ShaderMaterial` compartilhado em `project/materials/blocks/`.

Os PNGs são importados sem perda e sem mipmaps. O shader usa `filter_nearest` e `repeat_enable`; isso preserva os pixels e evita suavização de amostras nas bordas dos tiles. Em distâncias muito grandes, a filtragem nearest pode cintilar porque os mipmaps estão desativados.
