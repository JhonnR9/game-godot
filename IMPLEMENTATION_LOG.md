# Implementação do MVP voxel

## T00 — referência congelada (2026-10-16)

- Godot instalado: `4.7.2.stable.official.ed1daf0bf`.
- `godot-cpp`: commit `507ed9d840c01a3c5b2a39af8bb4000bfac30bf5`.
- Renderer efetivo: `gl_compatibility` (desktop e mobile). A feature string anterior declarava incorretamente `Forward Plus` e foi removida; a versão mínima registrada continua `4.7`.
- Comandos do repositório: `./build.sh` para a extensão; `./test.sh` para testes nativos e script Godot.
- Referências geométricas existentes no plano: seed 1337/chunk (0,0,0) = 1.946 sólidos, 21.492 triângulos sem junções, 51.212 com junções; seed 1360/chunk (3,0,11) ≈50.770 triângulos. São fixtures informadas pelo projeto, não medições de tempo ou FPS.
- O workspace já continha alterações do usuário em geometria, geração, cenas, testes e documentação. Elas foram preservadas.
- Limitações conhecidas: ainda não há runtime de jogo, streaming, player, inventário, persistência, UV/texturas de terreno ou pipeline multichunk; o mesher atual trata exterior do chunk como vazio.

## T01 — coordenadas globais iniciais

- `src/voxel_chunk.h` centraliza `floor_div`, `floor_mod`, conversão célula/chunk e centro físico BCC em coordenadas globais.
- `src/world_types.h` define IDs persistentes para os oito blocos do plano, registro simples de propriedades, descriptor versionado e domínio vertical operacional.
- O núcleo continua em C++ puro e sem tipos Godot.
- **Concluída.** Teste puro valida floor-div/mod, ida/volta incluindo negativas, offsets físicos entre chunks, paridade, unicidade/reversibilidade dos 14 vizinhos, IDs do catálogo e limites/bedrock.
- `WorldRepository::read` confirma que as camadas `-128/-127` e o fundo virtual abaixo delas são sólidos conhecidos, enquanto acima de `127` é ar conhecido.
- `bash test.sh`: T01, T02, geração/mesh, joiner, AO e smoke test Godot passaram (2026-10-16).

## T02 — repositório, snapshots e dependências

- Implementados em `src/core/world/`: snapshots `shared_ptr<const ...>` copy-on-write, revisões, edit overlay atômico, handles `{slot,generation,key}`, ACK idempotente de save e descarte apenas de slots limpos.
- `Unknown` é retornado para dados não residentes; limites verticais são respostas sólidas/ar conhecidas. O repositório sela as duas camadas inferiores como bedrock ao publicar e impede editar/substituir blocos protegidos.
- `SnapshotAssembler` deduplica e ordena dependências; limites conhecidos não viram requisitos desconhecidos.
- `DependencyStencil` enumera candidatos das seis molduras usando os vértices aprovados da ligação e bounds dos octaedros, inclui suportes/exposição/centro e possíveis recortadores; calcula chaves de chunks para ambas as paridades e suporta fronteiras negativas.
- Suíte pura testa imutabilidade, revisão obsoleta, ACK parcial, proteção de dirty, reciclagem de handle, Unknown, bedrock e stencil. Módulo incluído no build da extensão.
- **T02 concluída.** O stencil é conservador por AABB; o detalhamento das validações geométricas de T03 está registrado na seção seguinte.

## T03 — superfície multichunk

- Centralizadas as seis molduras em `src/bridge_topology.h`; o joiner aprovado e o stencil agora leem a mesma topologia, sem alterar os resultados geométricos de referência.
- `core/geometry/surface_contracts.*` define IDs globais estáveis para faces/caras de ligação, owner lexicográfico de suportes, vértices globais quantizados em coordenadas de lattice e assinaturas canônicas de segmentos.
- `SurfaceBuildService` consulta vizinhos reais, cria faces de células e pontes, atribui owner global, recorta por células/pontes concorrentes e retorna `BoundarySignature`; resultado não fica pronto enquanto houver dependência `Unknown`.
- Payload de malha agrupa por material, mantém `PrimitiveId` por triângulo e inclui posições/normais. BVH puro retorna procedência e material no raycast.
- Testes cobrem fronteira de chunks, seis molduras, layouts atravessando faces/arestas, incidência de arestas fechadas, volumes de fixture e picking. `bash build.sh` e `bash test.sh` passaram; os fixtures geométricos aprovados permanecem inalterados.
- Limite: o comparativo exaustivo de superfície 2×2×2 contra uma única referência monolítica e a integração do payload no renderer Godot continuam como validações futuras; publicação incremental fica em T06.
- **T03 concluída** para os contratos e critérios geométricos implementados.

## T04 — texturas e apresentação

- Oito texturas repetíveis 64×64 geradas estão em `project/textures/blocks/`.
- `MeshSurfacePayload` agora inclui UVs por vértice. Projeção usa base ortonormal determinística, período físico `4*SCALE` e fase global rebaseada por polígono, evitando perda de precisão em coordenadas distantes.
- Teste de superfície confere cardinalidade UV/posição e valores finitos; toda a suíte, incluindo smoke test Godot, passou depois da alteração.
- Renderer de preview agora agrupa superfícies por Grass/Dirt/Stone, aplica recursos ShaderMaterial compartilhados e UVs planos ortonormais. O shader usa textura do bloco e multiplica somente AO em vértice em tons de cinza; removeu-se o brilho aleatório por célula que gerava manchas quadradas.
- Criados materiais compartilhados para os oito IDs de bloco e shader nearest/repeat. O preview atual usa os três materiais que o gerador legado produz; os outros cinco já estão prontos para o catálogo completo.
- Projeção extraída para `src/planar_uv.h`, comum ao payload C++ e ao adaptador. Testes verificam a mesma densidade UV em planos quadrado/hexagonal, continuidade periódica em chunks negativos/densos e UV por vértice. A cena Godot confirma materiais/texturas, superfícies, AO separado e contagem de triângulos; screenshot do preview foi inspecionado.
- `bash test.sh` e `bash build.sh` passam, com smoke test Godot. **T04 concluída** para o renderer e os materiais catalogados.
- Auditoria de imagem: os PNGs estavam em compressão VRAM com perda (`lossy_quality=0.7`) e mipmaps lineares ativados. Os oito imports foram reconfigurados para lossless e sem mipmaps; o shader mantém `filter_nearest, repeat_enable`. Smoke test confirma 64×64, zero mip levels e sampler nearest/repeat. Captura do preview foi inspecionada após reimportação.
- Auditoria de upload: `triangle_count` acumulava as contagens entre regenerações e reportava índices CPU em vez dos índices presentes nos arrays aceitos pelo `ArrayMesh`. Agora é zerado a cada upload, conta os índices de cada superfície de volta do `ArrayMesh` e expõe triângulos descartados por dados inválidos. Teste cobre uploads repetidos e exige contagem igual, sem descartes; faceted=21.492 e conectado=50.770 no fixture-base.
- `bash build.sh` e a suíte completa `bash test.sh` passaram após essas correções (2026-10-16).

## T05 — runtime de jobs e filas limitadas

- `core/runtime/JobScheduler` fornece workers persistentes, filas limitadas por prioridade, coalescência por chave/revisão, rejeição de revisão antiga, consulta `is_current`, cancelamento por chave e shutdown com join. Exceção de tarefa não derruba o worker.
- `SurfaceJobQueue` conecta snapshots imutáveis a `SurfaceBuildService`, reserva créditos de resultado por contagem e bytes, coalesce por chunk/revisão e só publica payload atual. Workers não acessam Godot; o consumidor recebe resultado por `take`/`wait_take`.
- Métricas incluem filas, execução, submissão, coalescência, rejeição, cancelamento, conclusão, reservas e falhas/oversize. Testes cobrem capacidade, cancelamento, shutdown e payload idêntico com 1 e 4 workers usando fixture sólida.
- `bash test.sh` e `bash build.sh` passam. **T05 concluída** como runtime nativo de jobs; commit/publicação no renderer principal fica em T06, junto da aplicação consistente de mesh/colisão.

## T06 — aplicação incremental e colisão (em andamento)

- `TerrainChunkRenderer` cria um `StaticBody3D` e `CollisionShape3D` reutilizáveis. A shape côncava é derivada do mesmo `ArrayMesh` depois do upload; chunk sem triângulos limpa mesh e colisão. O nó de colisão é mantido durante regenerações.
- Smoke test Godot verifica que o número de vértices de face da shape corresponde ao total de triângulos da mesh, lança um raycast físico sobre a superfície e verifica que chunk vazio remove shape antiga. A extensão compila e o teste headless passa.
- **T06 segue em andamento:** ainda faltam aplicação por partes/commit atômico, pooling de buffers/nós, grupo de chunks em fronteira e teste físico de repouso/cruzamento entre chunks. O preview permanece síncrono e de chunk isolado.
