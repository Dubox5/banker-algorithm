# Algoritmo do Banqueiro (Banker's Algorithm)

Implementação em C do Algoritmo do Banqueiro para evitar deadlock, baseada no livro
*Operating System Concepts* (Silberschatz et al., 10ª edição, página P-45), com as
modificações pedidas na disciplina de Infraestrutura de Software (CESAR School).

**Autor:** Eduardo Boxwell Simões (ebs2@cesar.school)

## Requisitos

- Linux, Unix ou macOS (testado no Ubuntu 24.04 via WSL)
- `gcc` e `make`

## Compilação

```bash
make          # gera o executável ./banker
make clean    # remove os arquivos compilados
```

## Execução

Cada argumento é o número de instâncias de um tipo de recurso:

```bash
./banker 10 5 7
```

O programa lê `customer.txt` e `commands.txt` da pasta atual e grava o resultado em `result.txt`.

## Arquivos de entrada

**customer.txt**: uma linha por cliente, com a demanda máxima de cada recurso separada
por vírgula. O número de linhas define o número de clientes.

```
7,5,3
3,2,2
9,0,2
2,2,2
4,3,3
```

**commands.txt**: um comando por linha.

| Comando | Significado |
|---|---|
| `RQ <cliente> <r0> <r1> ...` | o cliente pede recursos |
| `RL <cliente> <r0> <r1> ...` | o cliente libera recursos |
| `*` | imprime as matrizes MAXIMUM, ALLOCATION, NEED e o vetor AVAILABLE |

## Saída (result.txt)

| Situação | Mensagem |
|---|---|
| Pedido atendido | `Allocate to customer C the resources ...` |
| Liberação feita | `Release from customer C the resources ...` |
| Pedido maior que o NEED | `The customer C request ... was denied because exceed its maximum allocation` |
| Pedido maior que o AVAILABLE | `The resources ... are not enough to customer C request ...` |
| Pedido levaria a estado inseguro | `The customer C request ... was denied because result in an unsafe state` |
| Liberação maior que o alocado | `The customer C release ... was denied because exceed its allocation` |

## Tratamento de erros

Toda a entrada é validada **antes** de qualquer processamento. Se houver erro, o programa
mostra uma mensagem em `stderr` (com o número da linha, quando aplicável), encerra com
código de saída 1 e **não** gera o `result.txt`. São detectados:

- nenhum argumento, ou argumento não numérico ou negativo;
- quantidade de argumentos diferente da quantidade de recursos em `customer.txt` ou `commands.txt`;
- `customer.txt` ou `commands.txt` inexistente ou sem permissão de leitura;
- `customer.txt` vazio, com letras, campos vazios, sem vírgulas ou com linha vazia entre clientes;
- `commands.txt` com comando desconhecido, parâmetros faltando ou não numéricos, cliente inexistente ou `*` seguido de texto.

Arquivos criados no Windows (quebra de linha CRLF) são aceitos.

## Testes

Com `customer.txt`, `commands.txt` e `result_esperado.txt` (a saída do enunciado) na pasta, rode:

```bash
./testes.sh
```

O script roda 26 cenários (execução normal, 22 casos de erro e 3 casos válidos extras) e grava
tudo em `evidencias.log`. O primeiro teste compara a saída com o resultado esperado do enunciado.

## Estrutura

| Arquivo | Descrição |
|---|---|
| `banker.c` | código-fonte |
| `Makefile` | compilação (`make`) e limpeza (`make clean`) |
| `testes.sh` | script de testes |
| `evidencias.log` | saída dos testes |
| `README.md` | este arquivo |
| `ebs2.pdf` | relatório |
