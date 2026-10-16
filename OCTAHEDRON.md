# Octaedro truncado em C++ / GDExtension

Projeto: `/home/jhonn/Documents/projects/game-godot`.
Cena principal: `project/truncated_octahedron.tscn`. Execute com F6 ou F5.
Arraste o mouse para orbitar, use a roda para zoom e R para restaurar a câmera.

## Construção e cálculos

Partimos do octaedro `|x| + |y| + |z| <= 3` e cortamos seus seis vértices
pelos planos `x = ±2`, `y = ±2`, `z = ±2`.
As interseções são as 24 permutações com sinais de `(0, 1, 2)`, declaradas
explicitamente em `src/truncated_octahedron_geometry.h`, junto das 14 faces.

Na base, a distância entre vértices vizinhos é `sqrt(2)`.
Para obter aresta `a`, cada coordenada é multiplicada por `a / sqrt(2)`.

- 24 vértices geométricos, 36 arestas, 14 faces (6 quadrados e 8 hexágonos regulares).
- Euler: `24 - 36 + 14 = 2`.
- Triângulos: `6 × 2 + 8 × 4 = 44` (132 índices).
- Área: `A = (6 + 12 sqrt(3)) a²`.
- Volume: `V = 8 sqrt(2) a³`.
- Raio circunscrito: `R = sqrt(5/2) a`.
- Para `a = 1,4`: `A ≈ 52,497835`, `V ≈ 31,044816`.

`TruncatedOctahedron` herda de `MeshInstance3D`. A malha é criada em C++ com
`ArrayMesh`, posições, normais e índices. Os vértices são duplicados entre
faces para preservar as normais planas: 72 entradas de renderização, mas
24 posições únicas. As propriedades `edge_length` e `color` aparecem no
Inspector. O intervalo de aresta aceito é 0,01 a 100; valores inválidos
preservam o tamanho anterior.

A ordem dos índices é horária vista do exterior, conforme a documentação
oficial: https://docs.godotengine.org/en/stable/classes/class_arraymesh.html
A normal externa usa `(c-a).cross(b-a)` devido a essa convenção.
Os polígonos convexos são triangulados em leque, sem vértices de centro.

O GDScript da demonstração controla apenas a câmera e a captura de imagem;
a construção do sólido fica integralmente na GDExtension C++.

## Compilar e testar

Na raiz do projeto:

```bash
scons platform=linux target=template_debug -j4
godot --headless --path project --editor --import --quit
godot --headless --path project --script res://tests/test_octahedron.gd
godot --path project
```

O teste examina a malha produzida pela biblioteca nativa: contagens, arestas
iguais, coplanaridade, incidência/direção das arestas, Euler, normais externas,
ordem dos triângulos, área e volume em quatro escalas (0,01; 1,4; 2; 100).
Também testa tamanho inválido, alteração de cor e instanciação da cena.
A coplanaridade é calculada pelas posições, pois Godot compacta as normais.

Compilação e execução verificadas em Godot 4.7.2, Linux x86_64, renderer
Compatibility/OpenGL. A captura PNG foi produzida pelo próprio Godot.
