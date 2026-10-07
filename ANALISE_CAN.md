# Análise do BCM e comunicação com TJA1050

## Conclusão

O projeto usa o controlador TWAI interno do ESP32 e o TJA1050 como
transceptor físico. O TJA1050 não é um controlador SPI e não precisa de
uma biblioteca própria. O caminho correto é TWAI TX -> TXD do TJA1050,
RXD do TJA1050 -> TWAI RX, com adequação dos níveis elétricos.

Há defeitos confirmados no firmware, corrigidos neste checkout. Sem logs
do dispositivo, esquema da montagem e acesso ao barramento físico, não é
possível afirmar qual deles, ou qual condição elétrica, causou a ausência
de comunicação observada.

## Defeitos e correções

1. **Vida útil do frame TX:** `can_bcm_send_status()` entregava ao driver
   ponteiros para `frame` e `d[8]` na pilha. A API TWAI nova é assíncrona;
   ambos devem permanecer válidos até a conclusão, inclusive quando o
   barramento está ocupado. Agora são persistentes e um semáforo impede
   sobrescrever o payload antes de `on_tx_done`. Um mutex serializa chamadas.
   Se ainda houver um envio pendente, o próximo status é descartado com
   `ESP_ERR_TIMEOUT`, sem modificar os dados que o driver está usando.
2. **Pinos opcionais omitidos:** os campos `quanta_clk_out` e
   `bus_off_indicator` ficavam em zero. Zero é GPIO0, não um pino desativado.
   O driver conecta saídas opcionais a esse pino. Agora ambos usam
   `GPIO_NUM_NC` (-1), evitando alteração involuntária do GPIO0.
3. **Sem tratamento de bus-off:** erros de barramento podem desligar o
   controlador. A tarefa RX agora consulta o estado mesmo sem receber
   mensagens e chama `twai_node_recover()` uma vez por episódio observado.
   A recuperação depende de o hardware observar o barramento recessivo;
   o código não declara sucesso enquanto o controlador não voltar a ativo.
   Isso não corrige uma ligação elétrica defeituosa.
4. **Envio enfileirado confundido com envio confirmado:** `ESP_OK` em
   `twai_node_transmit()` só confirma aceitação pelo driver. Agora
   `on_tx_done` distingue sucesso de falha e `on_error` conta erros de ACK.
   O monitor mostra estado, TEC, REC, erros, `TX_ACK`, `TX_falha`,
   `ACK_erros`, flags do último erro e perdas da fila RX.
5. **RX aceitava mensagens sem dados:** uma requisição remota RTR com
   ID 0x100 podia ser tratada como velocidade, copiando um buffer local
   não inicializado. Agora o frame/buffer são inicializados e somente
   frames clássicos de dados, standard, ID 0x100 e DLC 1..8 são processados.
   Há filtro explícito para 0x100 e contagem de perdas da fila.
6. **Estado compartilhado:** a velocidade era uma variável comum escrita
   pela tarefa RX e lida pela tarefa BCM. Agora usa operações atômicas C11.
7. **Falhas de inicialização:** os recursos CAN são liberados se a
   inicialização falhar. A criação das duas tarefas é verificada.

O loopback foi desabilitado para operação normal. **Não era um modo que
isolava o barramento**: nesta API, loopback também transmite externamente
e não gera seu próprio ACK. Desabilitá-lo sozinho não resolveria ausência
de ACK. O modo self-test continua desabilitado, preservando a necessidade
de outro controlador ativo confirmar os frames.

O bitrate continua **500 kbit/s**, agora centralizado em `CAN_BITRATE`.
Foi mantida a política de uma tentativa por status (single-shot, já
implícita no valor zero original de `fail_retry_cnt`), evitando retransmitir
indefinidamente um status velho. Os IDs, o payload e os pinos foram
preservados: TX GPIO15, RX GPIO36, status 0x200 e velocidade 0x100.

## Revisão por módulo

| Arquivos | Função e resultado da análise |
| --- | --- |
| `main/can_bcm.c`, `.h` | Transporte CAN, payload e tarefa RX. Correções acima; formato 0x200 preservado, com bytes 5..7 zerados. |
| `main/bcm_config.h` | Mapeamento de pinos, IDs e períodos. Não há sobreposição dos GPIOs CAN com os periféricos declarados. GPIO36 é entrada apenas, adequado para RX. GPIO15 é strapping: o circuito externo deve respeitar os níveis de boot. Não troquei pinos sem conhecer a montagem. |
| `main/bcm_esp32.c` | Inicialização, inputs, TX e painel. CAN só começa depois dos demais periféricos. `ESP_ERROR_CHECK` pode reiniciar antes do CAN se um periférico obrigatório falhar. Criação de tarefas agora verificada. O TX nominal de 100 ms compartilha o loop com OLED, ADC e buzzer, portanto não é um período estrito. |
| `main/adc_shared.c`, `.h` | Um handle ADC1 compartilhado; evita criar a unidade duas vezes. Não configura pinos CAN. Falha fatal na inicialização impede chegar ao CAN. |
| `main/luminosidade.c`, `.h` | ADC GPIO34, média de 8 leituras, byte 0. Retornos das leituras ADC não são verificados, podendo fornecer valores incorretos; não explica sozinho ausência de frames. |
| `main/velocidade_limpador.c`, `.h` | ADC GPIO35, byte 1; mesma observação sobre erros de leitura. |
| `main/dimmer_interno.c`, `.h` | Estado 0..255, saturação e conversões; compõe byte 2. Não controla o transceptor. |
| `main/seta.c`, `.h` | Estados e temporização das setas; bits 0 e 1 do byte 3. Não bloqueia CAN. |
| `main/pisca_alerta.c`, `.h` | Estado do alerta, bit 3 do byte 3. A fase do buzzer consulta `seta_get_blink_phase`, que para se ambas as setas estiverem desligadas; problema visual/sonoro separado. |
| `main/trava_portas.c`, `.h` | Estado das portas, bit 2 do byte 3; auto-lock depende do byte recebido em 0x100. |
| `main/farol.c`, `.h` | Estado 0..3 no byte 4; não interage com TWAI. |
| `main/velocidade_veiculo.c`, `.h` | Estado recebido entre tarefas, agora atômico. Não há timeout de validade da velocidade: auto-lock pode usar informação antiga após desconexão, questão funcional separada. |
| `main/botao.c`, `.h` | GPIOs 4/5/18/19 com debounce; cabeçalho descreve soltura, mas implementação produz clique na pressão. Sem conflito CAN. |
| `main/encoder.c`, `.h` | GPIOs 16/17/23. Polling recomendado de 2 ms, mas loop BCM usa pelo menos 20 ms; pode perder passos. Em módulos ESP32 com PSRAM, verificar disponibilidade de 16/17. Não explica ausência de CAN. |
| `main/leds.c`, `.h` | GPIOs 12/13/14/25/26/27/32/33; sem conflito CAN. GPIO12 é strapping e merece cuidado na montagem. Atualização usa o modo do ciclo anterior, causando um pequeno atraso visual. |
| `main/buzzer.c`, `.h` | GPIO2; pulsos bloqueantes de 30/80 ms atrasam o loop e o TX. Não desligam TWAI. GPIO2 também participa do boot. |
| `main/oled_display.c`, `.h` | I2C GPIO21/22. Erros de envio de comandos/dados são ignorados; display ausente pode produzir timeouts repetidos e atrasar o envio CAN. Erros de criação do barramento/dispositivo abortam a inicialização antes do CAN. |
| `CMakeLists.txt`, `main/CMakeLists.txt` | Projeto ESP-IDF, componentes GPIO/I2C/TWAI/ADC/RTOS. Build real com IDF 6.0.2 passou. A opção `-Wno-error=cpp` é legado e não resolve problemas de comunicação. |
| `sdkconfig`, `sdkconfig.defaults`, `.old` | Target ESP32. Defaults configuram tick de 1 kHz e stack principal. Não alimentam nem habilitam eletricamente o TJA1050. |
| Arquivos de build, logs, binários, mapas, JSONs e `4.0.3/` | Artefatos de compilação anteriores com caminhos Windows; não são outro código BCM nem comprovação destas correções. O `CMakeCache.txt` na raiz impede configurar diretamente ali. Não sobrescrevi esses artefatos. |

Os problemas periféricos citados são achados de revisão; somente os
arquivos indicados nas correções foram alterados. O código não implementa
controle do pino S do TJA1050; ele precisa estar corretamente ligado no
circuito.

## Verificação da montagem

Para o **TJA1050 original**, conferir o datasheet e o esquema do módulo:

- VCC requer 5 V (faixa nominal do CI 4,75..5,25 V). Alimentar o CI em
  3,3 V não é uma configuração suportada.
- GND do ESP32, do transceptor e do outro nó devem compartilhar referência.
- GPIO15 -> TXD (pino 1 do CI); RXD (pino 4) -> GPIO36 **com adaptação
  de nível quando necessária**. O RXD do TJA1050 alimentado em 5 V pode
  exceder os limites do ESP32. Não assumir que o módulo tem conversão:
  conferir o esquema ou medir. O TJA1050 aceita entrada TXD de lógica
  3,3 V nas condições especificadas, mas isso não torna RXD uma saída 3,3 V.
- S (pino 8) baixo seleciona operação normal; alto seleciona silent,
  impedindo transmissão. Não confundir com o pino de standby de outro CI.
- CANH (pino 7) -> CANH; CANL (pino 6) -> CANL do outro nó. Usar par
  trançado e verificar comprimento/topologia compatíveis com 500 kbit/s.
- Terminação de 120 ohms em cada extremidade física. Com todos os nós
  desligados, duas terminações em paralelo dão aproximadamente 60 ohms
  entre CANH e CANL. Verificar resistores já presentes nos módulos.
- É necessário outro **controlador CAN ativo** em 500 kbit/s, CAN clássico.
  Um segundo TJA1050 sem controlador ou um adaptador em listen-only não
  fornece ACK. O TJA1050 sozinho não recebe/decodifica mensagens de aplicação.

## Validação executada e roteiro de bancada

Passaram:

- Build completo do ESP32 com ESP-IDF **v6.0.2** e defaults do projeto,
  em uma cópia limpa das fontes. Imagem gerada:
  `/workspace/bcm-validation-build/bcm_esp32.bin` (0x33970 bytes).
- `bash tests/run_host_tests.sh`: quatro grupos de testes C sobre o código
  real do módulo, com driver/RTOS simulados: inicialização e limpeza;
  payload e vida útil assíncrona; RX válido/inválido e overflow;
  contabilização de ACK e solicitação de recuperação bus-off.
- `git -c core.whitespace=cr-at-eol diff --check` (preservando CRLF dos arquivos originais).

Esses testes não reproduzem temporização de ISR, níveis elétricos,
arbitragem real ou conclusão física da recuperação. Não houve flash nem
teste em placa neste ambiente. A imagem antiga versionada na raiz **não
contém** estas correções.

Na bancada:

1. Gravar um build novo completo (bootloader, partition table e aplicação,
   conforme `flash_args` gerado) e abrir o monitor serial.
2. Conectar outro nó ativo/USB-CAN em 500 kbit/s. Confirmar frames standard
   0x200 com DLC 8 e layout de `main/can_bcm.h`. Alterar entradas e verificar
   que os bytes correspondentes mudam. `TX_ACK` deve crescer.
3. Enviar frame standard de dados `0x100`, DLC 1, byte `0x2A`. Esperar log
   de 42 km/h, indicação no painel e auto-lock quando aplicável.
4. Se `ACK_erros` ou `TX_falha` crescerem, conferir primeiro outro nó ativo,
   bitrate, S, alimentação, GND, CANH/CANL e terminação. TEC/REC e flags
   adicionais ajudam a localizar erro, mas não identificam sozinhos a peça.
5. Se o controlador entrar em bus-off, observar solicitação de recuperação
   e retorno a ativo após corrigir a condição do barramento. Ausência de ACK
   sozinha não necessariamente provoca bus-off: pode permanecer error-passive.
6. Se não aparecer `TWAI pronto`, localizar o erro anterior na inicialização
   de NVS/ADC/OLED/CAN; isso distingue falha de startup de erro no barramento.

Para recompilar neste ambiente sem reutilizar o cache Windows:

```bash
export IDF_TOOLS_PATH=/workspace/.idf-tools
# Nesta máquina, as dependências Python foram instaladas do PyPI com a
# opção oficial --no-constraints, pois dl.espressif.com está bloqueado.
# Este ajuste desativa a checagem de versões Python, não TLS/checksums.
export IDF_PYTHON_CHECK_CONSTRAINTS=no
source /workspace/esp-idf/export.sh
mkdir -p /workspace/bcm-validation-source
cp /workspace/bcmwill/CMakeLists.txt /workspace/bcmwill/sdkconfig.defaults /workspace/bcm-validation-source/
cp -a /workspace/bcmwill/main /workspace/bcm-validation-source/
cd /workspace/bcm-validation-source
idf.py -B /workspace/bcm-validation-build \
  -D SDKCONFIG=/workspace/bcm-validation-build/sdkconfig build
```

Não reutilizar CMakeCache, `.elf` ou `.bin` anteriores. Para validação de
produção, também compilar com a configuração efetivamente usada na placa.

Referências:

- [Espressif TWAI, IDF v6.0.2](https://docs.espressif.com/projects/esp-idf/en/v6.0.2/esp32/api-reference/peripherals/twai.html): vida útil dos ponteiros, loopback, ACK, filtros e recuperação.
- Headers/driver oficiais de `espressif/esp-idf` no tag v6.0.2, consultados
  localmente em `/workspace/esp-idf/components/esp_driver_twai`.
- [NXP TJA1050 datasheet](https://www.nxp.com/docs/en/data-sheet/TJA1050.pdf),
  referência para conferir a montagem específica; download bloqueado neste
  ambiente. Sem esquema/medição do módulo, sua adequação elétrica não foi validada.
