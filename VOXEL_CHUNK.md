# Chunk voxel de octaedros truncados

Projeto: `/home/jhonn/Documents/projects/game-godot`.
Cena de demonstração: `project/voxel_chunk.tscn` (cena principal).
Classe nativa: `OctahedralVoxelChunk`, derivada de `MeshInstance3D`.

## Controles no editor

Selecione o nó **Chunk** na cena. No Inspector:

| Propriedade | Efeito |
| --- | --- |
| Size X | Quantidade de células por linha em X, entre 1 e 64 |
| Size Y | Quantidade de camadas verticais, entre 1 e 64 |
| Size Z | Quantidade de células por linha em Z, entre 1 e 64 |
| Edge Length | Comprimento da aresta de cada octaedro, de 0,01 a 100 |
| Color | Cor base dos voxels |
| Color Variation | Variação de brilho entre voxels; zero desativa |

A malha se atualiza automaticamente no editor. Alterações feitas no mesmo
quadro são agrupadas em uma única reconstrução. Os tamanhos são contagens
de voxels: `Size X × Size Y × Size Z` é a capacidade total.
**Redimensionar recria o chunk completamente preenchido.** Mudar a aresta
ou a cor preserva as células vazias. O padrão é 8 × 4 × 8 = 256 voxels.

O chunk é centralizado em X/Z, com a base em Y=0. F5 executa a demonstração;
arraste para orbitar, use a roda para zoom e R para enquadrar a malha.
A câmera da demonstração é reenquadrada quando o chunk muda.

Se o Godot já estava aberto antes da compilação, reabra o projeto para
carregar a nova classe: a GDExtension existente usa `reloadable=false`.
Também é possível adicionar `OctahedralVoxelChunk` em qualquer cena pelo
diálogo de criação de nós.

## Encaixe geométrico

Os vértices base são as permutações com sinais de `(0,1,2)`. Para aresta `a`,
a escala é `s = a / sqrt(2)`. O octaedro tem 6 faces quadradas e 8 hexagonais.

Não basta colocar esses sólidos em uma grade cúbica simples. Os centros
formam uma rede cúbica de corpo centrado (BCC), indexada por camadas:

```
p = y % 2
centro(x,y,z) = s × (4x + 2p, 2y, 4z + 2p)
```

A implementação subtrai ainda uma origem comum para centralizar X/Z e
apoiar o chunk no chão. Camadas ímpares deslocam os centros em `2s` em X/Z.
Os passos horizontais são `4s`; a distância vertical entre camadas é `2s`.
Por isso três contagens iguais não produzem dimensões físicas iguais.

Cada célula tem 14 vizinhos de face:

- 6 pelas faces quadradas: deslocamentos físicos `(±4s,0,0)`, `(0,±4s,0)`, `(0,0,±4s)`;
- 8 pelas hexagonais: todas as combinações de `(±2s,±2s,±2s)`.

As posições dos vértices são somadas em coordenadas inteiras antes da
escala; vértices compartilhados recebem posições idênticas. O resultado
preenche o espaço sem lacunas entre células vizinhas.

## Malha e dados

Todo o chunk é uma única `ArrayMesh` com uma superfície. Apenas faces com
vizinho vazio ou fora do chunk são emitidas. Não há um nó por voxel.
As normais são planas e o brilho varia deterministicamente por célula.

A ocupação é serializada em `voxel_data`; a malha derivada não é salva
na cena e é reconstruída ao carregar. Índice linear:
`x + size_x * (z + size_z * y)`.

API disponível em C++ e GDScript:

```gdscript
$Chunk.set_chunk_size(Vector3i(12, 6, 12))
$Chunk.set_voxel(Vector3i(3, 2, 3), false) # remove uma célula
$Chunk.set_voxel(Vector3i(3, 2, 3), true)  # recoloca
$Chunk.fill(false)                       # esvazia
$Chunk.fill(true)                        # preenche
var center = $Chunk.get_voxel_center(Vector3i(3, 2, 3))
```

`get_voxel()` retorna falso fora dos limites. `get_neighbor(cell, face)`
usará a ordem das faces em `truncated_octahedron_geometry.h`.
`rebuild_mesh()` força uma atualização imediata quando necessário;
normalmente a atualização é adiada para o fim do quadro.
O sinal `mesh_rebuilt` indica que a malha e as estatísticas foram atualizadas.

Esta implementação cobre um chunk isolado. Faces nas bordas são mantidas;
a comunicação entre chunks, streaming e colisões não estão implementados.

## Compilação e testes

Na raiz do projeto:

```bash
scons platform=linux target=template_debug -j4
godot --headless --path project --editor --import --quit
godot --headless --path project --script res://tests/test_voxel_chunk.gd
godot --headless --path project --script res://tests/test_octahedron.gd
godot --headless --path project --editor res://tests/editor_chunk_probe.tscn -- --verify-chunk-editor
godot --path project
```

Os testes verificam encaixes quadrados e hexagonais, múltiplas paridades de
camada, remoção de faces internas, ausência de fissuras e de triângulos
duplicados, área, volume, cavidades, escalonamento, dados fora dos limites,
chunk vazio, gravação/carregamento e atualização automática no editor.

A classe é registrada com `GDREGISTER_CLASS`, e as propriedades são
publicadas com `ADD_PROPERTY` conforme a API do Godot:
https://docs.godotengine.org/en/stable/tutorials/scripting/cpp/gdextension_cpp_example.html
