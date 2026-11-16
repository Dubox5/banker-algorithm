#!/bin/bash
# testes.sh - roda o banker em varios cenarios e gera o evidencias.log
# Uso: ./testes.sh   (dentro da pasta com banker.c, Makefile, customer.txt e commands.txt)

LOG="$(pwd)/evidencias.log"
BIN="$(pwd)/banker"
ORIG="$(pwd)"

make -s || { echo "Falha na compilacao"; exit 1; }

TMP=$(mktemp -d)
trap 'chmod -R u+rw "$TMP"; rm -rf "$TMP"' EXIT

# prepara um diretorio limpo com os arquivos originais
reset_files() {
    cp "$ORIG/customer.txt" "$ORIG/commands.txt" "$TMP/"
    rm -f "$TMP/result.txt"
}

# executa um caso de teste: run "descricao" argumentos...
N=0
run() {
    N=$((N + 1))
    local desc="$1"; shift
    {
        echo "------------------------------------------------------------"
        echo "TESTE $N: $desc"
        echo "\$ ./banker $*"
        (cd "$TMP" && "$BIN" "$@" 2>&1)
        echo "[codigo de saida: $?]"
        if [ -f "$TMP/result.txt" ]; then
            echo "[result.txt gerado:]"
            cat "$TMP/result.txt"
        else
            echo "[result.txt NAO foi gerado]"
        fi
    } >> "$LOG"
}

{
    echo "EVIDENCIAS DE TESTE - Algoritmo do Banqueiro"
    echo "Data: $(date)"
    echo "Sistema: $(uname -srm)"
    echo "Compilador: $(gcc --version | head -1)"
} > "$LOG"

# ===== 1. Execucao normal (exemplo do enunciado) =====
reset_files
run "Execucao com a entrada do enunciado" 10 5 7
if diff -q "$TMP/result.txt" "$ORIG/result_esperado.txt" > /dev/null 2>&1; then
    echo "[COMPARACAO COM O RESULTADO ESPERADO: IDENTICO]" >> "$LOG"
else
    echo "[COMPARACAO COM O RESULTADO ESPERADO: DIFERENTE]" >> "$LOG"
fi

# ===== 2. Erros nos argumentos =====
reset_files; run "Nenhum argumento"
reset_files; run "Menos argumentos que recursos (2 em vez de 3)" 10 5
reset_files; run "Mais argumentos que recursos (4 em vez de 3)" 10 5 7 3
reset_files; run "Argumento nao numerico" 10 abc 7
reset_files; run "Argumento negativo" 10 -5 7

# ===== 3. Erros de arquivo inexistente / sem permissao =====
reset_files; rm "$TMP/customer.txt"; run "customer.txt inexistente" 10 5 7
reset_files; rm "$TMP/commands.txt"; run "commands.txt inexistente" 10 5 7
reset_files; chmod 000 "$TMP/customer.txt"; run "customer.txt sem permissao de leitura" 10 5 7
chmod 644 "$TMP/customer.txt"
reset_files; chmod 000 "$TMP/commands.txt"; run "commands.txt sem permissao de leitura" 10 5 7
chmod 644 "$TMP/commands.txt"

# ===== 4. Erros de formatacao no customer.txt =====
reset_files; : > "$TMP/customer.txt"; run "customer.txt vazio" 10 5 7
reset_files; printf '7,5,3\n3,2\n' > "$TMP/customer.txt"; run "customer.txt com cliente de 2 recursos" 10 5 7
reset_files; printf '7,5,a\n' > "$TMP/customer.txt"; run "customer.txt com letra" 10 5 7
reset_files; printf '7,,3\n' > "$TMP/customer.txt"; run "customer.txt com campo vazio" 10 5 7
reset_files; printf '7 5 3\n' > "$TMP/customer.txt"; run "customer.txt sem virgulas" 10 5 7
reset_files; printf '7,5,3\n\n3,2,2\n' > "$TMP/customer.txt"; run "customer.txt com linha vazia no meio" 10 5 7

# ===== 5. Erros de formatacao no commands.txt =====
reset_files; printf 'RQ 0 1 1\n' > "$TMP/commands.txt"; run "commands.txt com comando de 2 recursos" 10 5 7
reset_files; printf 'XX 0 1 1 1\n' > "$TMP/commands.txt"; run "commands.txt com comando desconhecido" 10 5 7
reset_files; printf 'RQ 9 1 1 1\n' > "$TMP/commands.txt"; run "commands.txt com cliente inexistente" 10 5 7
reset_files; printf 'RQ\n' > "$TMP/commands.txt"; run "commands.txt com RQ sem parametros" 10 5 7
reset_files; printf 'RQ 0 1 x 1\n' > "$TMP/commands.txt"; run "commands.txt com recurso nao numerico" 10 5 7
reset_files; printf '* 1\n' > "$TMP/commands.txt"; run "commands.txt com * seguido de texto" 10 5 7
reset_files; printf 'RQ 0 0 2 1\nXX\n' > "$TMP/commands.txt"; run "Erro na linha 2: nada e processado" 10 5 7

# ===== 6. Casos validos extras =====
reset_files; printf '7,5,3\r\n3,2,2\r\n9,0,2\r\n2,2,2\r\n4,3,3\r\n' > "$TMP/customer.txt"
run "customer.txt com quebra de linha do Windows (CRLF) - deve funcionar" 10 5 7
reset_files; printf 'RL 0 1 1 1\n*\n' > "$TMP/commands.txt"; run "Liberar mais do que o alocado" 10 5 7
reset_files; printf '5,5\n3,3\n' > "$TMP/customer.txt"; printf 'RQ 0 2 2\nRQ 1 1 1\n*\nRL 0 2 2\n*\n' > "$TMP/commands.txt"
run "Numero diferente de recursos (2) e clientes (2)" 6 6

echo "------------------------------------------------------------" >> "$LOG"
echo "Total de testes: $N" >> "$LOG"
echo "Pronto: $N testes gravados em evidencias.log"
