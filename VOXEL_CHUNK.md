# Terreno procedural — chunk lógico 16 × 16 × 16

Implementação integrada ao projeto `/home/jhonn/Documents/projects/game-godot`.
O chunk lógico, a geração procedural e a montagem da geometria são C++ puro;
o nó do Godot apenas controla os parâmetros e exibe a malha.

## Abrir e visualizar

Importe `project/project.godot` no Godot 4.7.2 e abra `voxel_chunk.tscn`.
A biblioteca Linux x86_64 já está compilada em `project/bin/linux`.
Selecione o nó **TerrainRenderer** e, no Inspector, altere:

- **Seed**: seed inteira do mundo, padrão 1337; aceita negativos.
- **Chunk Coordinates**: coordenadas X/Y/Z do chunk selecionado.

A atualização ocorre automaticamente no editor, sem executar o jogo.
O tamanho é sempre **16 × 16 × 16** e não é exposto no Inspector.
A aresta de cada octaedro é fixa em 0,5 unidade.

Experimente `Seed=1337`, coordenadas `(0,0,0)` para superfície e cavernas;
`(1,0,0)` para outra região; `(0,-1,0)` para o subsolo.
Y=1 geralmente é ar acima da superfície. Nesse caso, a malha anterior é
removida e a demonstração indica que a região está vazia.
F5 executa a demonstração; arraste para orbitar, roda para zoom, R para enquadrar.

**As coordenadas selecionam os dados do mundo.** A malha selecionada continua
centralizada perto da origem para facilitar a visualização e evitar perda
de precisão, mesmo quando as coordenadas do chunk são muito grandes.

## Responsabilidades separadas

| Classe | Responsabilidade | Depende do Godot? |
| --- | --- | --- |
| `VoxelChunk` | Array fixo de 4096 células, materiais, seed e coordenadas | Não |
| `GradientNoise` | Ruído de gradientes 2D/3D, interpolação quíntica e fBm | Não |
| `TerrainGenerator` | Usa seed/coordenadas para preencher os dados do chunk | Não |
| `OctahedronGeometry` | Cria faces, normais e triângulos de um octaedro | Não |
| `ChunkMesher` | Lê os dados, elimina faces internas e monta buffers da malha | Não |
| `TerrainChunkRenderer` | Controla os parâmetros do Inspector e aplica os buffers a `ArrayMesh` | Sim |

O fluxo é explícito:

```
seed + coordenadas
       ↓
TerrainGenerator → VoxelChunk (somente dados)
                         ↓
          ChunkMesher + OctahedronGeometry
                         ↓
                   MeshBuffers C++
                         ↓
                TerrainChunkRenderer
                         ↓
                  ArrayMesh / Godot
```

`VoxelChunk` não herda de `Node`, `Object` ou `Resource`, não usa tipos do
Godot e não cria geometria. Os testes da lógica compilam e executam sem
linkar `godot-cpp`. Só o adaptador de renderização é registrado no ClassDB.
A antiga classe de nó `OctahedralVoxelChunk` foi removida desta versão.

## Geração

Para coordenadas de chunk `C` e célula local `L`, a célula global é
`G = 16*C + L`. Os centros dos octaedros seguem a malha BCC de camadas
alternadas. Em unidades normalizadas, os ruídos recebem:

```
p = paridade positiva de G.y
P = (2*G.x + p, G.y, 2*G.z + p)
```

A superfície usa somente X/Z:

```
altura = 10 + 9 * fBm2D(P.x*0.055 + 0.37, P.z*0.055 + 0.71)
```

Acima dessa altura a célula é vazia. Abaixo, um fBm 3D com seed derivada
escava células quando seu valor supera 0,10. As cavernas só são escavadas
a pelo menos três camadas abaixo da superfície, preservando um teto sólido.
A superfície usa quatro oitavas; as cavernas usam três.

Os materiais são grama, terra e pedra, escolhidos pela profundidade. A
variação de cor nas faces inclinadas usa coordenadas globais e seed.
As faces quadradas e as conexões coplanares usam a mesma cor base para
evitar marcas em formato de voxel na superfície plana. O gerador é determinístico,
sem estado aleatório compartilhado e sem reiniciar o ruído a cada chunk.
Coordenadas negativas e seeds de 64 bits são suportadas; a interface do
Godot aceita as coordenadas inteiras de 32 bits de `Vector3i`.

Somente **um chunk** é gerado e exibido. Suas bordas são mantidas como um
corte fechado, para visualizar as cavernas e as camadas internas. Não há
streaming, colisões ou remoção de faces entre múltiplos chunks nesta etapa.

Ao salvar a cena, ficam apenas seed/coordenadas; nem a ocupação gerada nem
a malha são gravadas. Tudo é regenerado ao abrir. A mudança simultânea de
seed e coordenadas é agrupada em uma única atualização no fim do quadro.

## Compilar e testar

Para compilar com a configuração existente do projeto:

```bash
scons platform=linux target=template_debug -j4
bash test.sh
```

É possível alterar a localização das dependências:

```bash
GODOT_CPP_DIR=/caminho/godot-cpp bash build.sh
```

O `SConstruct` encontra os novos arquivos automaticamente. A lista do CMake
também foi atualizada. `build.sh` é uma alternativa que reutiliza o godot-cpp
previamente compilado; SCons é o fluxo principal.

Teste específico do editor:

```bash
godot --headless --path project --editor res://tests/aligned_surface_editor_probe.tscn -- --verify-terrain-editor
```

Resultados verificados nesta entrega:

- Compilação C++ da GDExtension e da lógica independente.
- Lógica pura testada com UBSan, incluindo limites de seed e coordenadas.
- Determinismo, mudança de seed/região, continuidade da amostragem global e coordenadas negativas.
- Superfície irregular 2D, cavernas 3D e proteção do teto.
- Superfície da malha fechada, sem triângulos duplicados e com volume correto.
- Propriedades no Inspector, atualização adiada, gravação/carregamento e limpeza de regiões vazias.
- Atualização da seed e das coordenadas em modo editor headless: PASS.
- Seed 1337, chunk (0,0,0): 1946 voxels sólidos e 553 células de cavernas.
  A malha original tem 21492 triângulos; com as conexões nas seis direções, 51212.


A configuração da GDExtension usa `reloadable=false`: feche e reabra o
editor após recompilar para carregar a biblioteca atualizada.

## Conexão local de quatro células alinhadas nas seis direções

O controle **Connect Aligned Surfaces** está ativado por padrão no nó
`TerrainRenderer`. A suavização anterior foi removida. Os octaedros mantêm
suas faces planas e suas pontas, com as mesmas normais facetadas.

Um grupo 2×2 recebe a ligação no topo, na base ou nas laterais quando suas
quatro células são sólidas, as faces naquela direção estão expostas e o
centro do vão está livre. A mesma construção dos oito vértices internos é
rotacionada ou refletida para +X, -X, +Y, -Y, +Z e -Z. O octógono fica no
mesmo plano das quatro faces; um losango recuado e triângulos fecham as
bordas. Nas laterais, as células se alinham a cada duas camadas verticais,
pois camadas consecutivas da malha BCC têm um deslocamento horizontal.
O sólido da ligação é definido por dez semiespaços planos; o recorte convexo
remove as partes dos polígonos originais que ficam enterradas na ligação.
Pontas fora da região de ligação não são movidas nem arredondadas.

Ligações vizinhas têm faces internas canceladas. As arestas são subdivididas
nos pontos de interseção para evitar frestas e junções em T. Normais por
polígono preservam o visual de prismas. Dados de ocupação, seed e coordenadas
continuam iguais: as ligações existem somente na geometria exibida.

Grupos incompletos ou desnivelados permanecem como eram. A regra atua nas
seis direções sem suavizar pontas nem preencher globalmente as cavernas
ou os vãos maiores.

## Oclusão ambiente após as conexões

A classe C++ pura `AmbientOcclusion` recebe os triângulos finais, depois de
`ChunkMesher` e `SurfaceJoiner`. Uma BVH acelera 32 raios de hemisfério por
combinação de posição/normal. As amostras têm distribuição cossenoidal;
o bloqueio perde influência com o quadrado da distância normalizada ao
alcance. Um pequeno deslocamento da origem do raio evita autossombra.
Vértices coplanares compartilhados recebem o mesmo fator de AO.

O fator escurece as cores dos vértices durante o envio ao Godot. É uma AO
visual pré-calculada: ela modula a cor do material e, portanto, afeta sua
aparência sob luz direta e ambiente. Não é um cálculo de iluminação global.
Amostras, seed e geometria iguais produzem o mesmo sombreamento.

No Inspector do `TerrainRenderer`, grupo **Ambient Occlusion**:

- **Strength**: de 0 a 1; padrão 0,8; zero desativa.
- **Radius**: de 0,05 a 5 unidades locais; padrão 1,25.

Seed, coordenadas, conexões e parâmetros de AO disparam o recálculo sobre a
malha atual. As novas superfícies planas não reutilizam sombras dos vãos
anteriores. A forma dos octaedros, os triângulos e a ocupação não mudam.
O cálculo considera este chunk; objetos externos e regiões fora dele não
participam da AO pré-calculada. Sombras dinâmicas da cena continuam separadas.
