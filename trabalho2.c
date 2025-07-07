#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>
#define SEED    0x12345678
#define INITIAL_TABLE_SIZE 1000
#define RESIZE_FACTOR 2.0f

typedef enum{
  LINEAR_PROBING,
  DOUBLE_HASHING
} HashType;

typedef struct {
  int ocupacao;
  double tempo_linear;
  double tempo_duplo;
  double tempo_insercao_linear;
  double tempo_insercao_duplo;
} resultado_teste;

typedef struct {
     uintptr_t * table;
     int size;
     int max;
     uintptr_t deleted;
     char * (*get_key)(void *);
     HashType type;           //hash a ser usando
     float load_factor_limit; //taxa de ocupação p redimensionar 
}thash;

//hash simples
uint32_t hashf(const char* str, uint32_t h){
    /* One-byte-at-a-time Murmur hash 
    Source: https://github.com/aappleby/smhasher/blob/master/src/Hashes.cpp */
    for (; *str; ++str) {
        h ^= *str;
        h *= 0x5bd1e995;
        h ^= h >> 15;
    }
    return h;
}

//hash duplo
uint32_t hashf2(uint32_t hash, int table_size){
  //primo mais proximo de 6100
  const int R = table_size > 6000 ? 5999 : 997;
  return (R - (hash % R));
}
int hash_constroi(thash *h, int nbuckets, char * (*get_key)(void *), HashType type, float load_factor);

//função para redimencionar 
void hash_resize(thash *h) {
  int old_max = h->max;
  uintptr_t *old_table = h->table;
  int new_max = (int) (old_max * RESIZE_FACTOR);

  printf("Redimencionamento tabela de %d para %d buckets.\n", old_max, new_max);

  //nova tab temporaria
  thash new_h;
  hash_constroi(&new_h, new_max, h->get_key, h->type, h->load_factor_limit);

  for(int i = 0; i < old_max; i++){
    if(old_table[i] != 0 && old_table[i] != h->deleted) {
      hash_insere(&new_h, (void *)old_table[i]);
    }
  }
  free(old_table);

  h->table = new_h.table;
  h->max = new_h.max;
  h->size = new_h.size;
}

//função para inserir
int hash_insere(thash *h, void *bucket){

  //primeiro verifica a LF e redimenciona se necessario
  if (((float)h->size / h->max) >= h->load_factor_limit){
    hash_resize(h);
  }

  uint32_t main_hash = hashf(h->get_key(bucket), SEED);
  int pos = main_hash % h->max;
  int step = 1; //padrao p sondagem linear

  if(h->type == DOUBLE_HASHING){
    step = hashf2(main_hash, h->max);
  }

  int original_pos = pos;
  do{
    if(h->table[pos] == 0 || h->table[pos] == h->deleted) {
      h->table[pos] = (uintptr_t)bucket;
      h->size++;
      return EXIT_SUCCESS;
    }
    pos = (pos + step) % h->max;

  }while(pos != original_pos);

  //se sair a tab ta cheia
  return EXIT_FAILURE;
}

//função de busca
void *hash_busca (thash h, const char *key) {
  uint32_t main_hash = hashf(key, SEED);
  int pos = main_hash % h.max;
  int step = 1;

  if(h.type == DOUBLE_HASHING) {
    step = hashf2(main_hash, h.max);
  }

  int original_pos = pos;
  do{
    if(h.table[pos] == 0) {
      return NULL;
    }

    if(h.table[pos] != h.deleted) {
      if(strcmp(h.get_key((void *)h.table[pos]), key) == 0) {
        return (void *)h.table[pos];
      }
    }
    pos = (pos + step) % h.max;
  }while(pos != original_pos);
  return NULL;
}

//função de remoção
int hash_remove(thash * h, const char * key){
  uint32_t main_hash = hashf(key, SEED);
  int pos = main_hash % h->max;
  int step = 1;

  if (h->type == DOUBLE_HASHING) {
    step = hashf2(main_hash, h->max);
  }

  int original_pos = pos;
  do {
    if (h->table[pos] == 0) {
      return EXIT_FAILURE; // Chave não encontrada
    }

    if (h->table[pos] != h->deleted) {
      if (strcmp(h->get_key((void *)h->table[pos]), key) == 0) {
        free((void *)h->table[pos]);
        h->table[pos] = h->deleted;
        h->size--;
        return EXIT_SUCCESS;
      }
    }
    pos = (pos + step) % h->max;
  }while (pos != original_pos);

  return EXIT_FAILURE;
}

// A função constrói agora aceita o tipo de hash e o fator de carga
int hash_constroi(thash *h, int nbuckets, char * (*get_key)(void *), HashType type, float load_factor) {
  h->table = calloc(sizeof(uintptr_t), nbuckets);
  if (h->table == NULL) {
    return EXIT_FAILURE;
  }
  h->max = nbuckets;
  h->size = 0;
  h->deleted = (uintptr_t)1;
  h->get_key = get_key;
  h->type = type;
  h->load_factor_limit = load_factor;
  return EXIT_SUCCESS;
}

// hash_apaga permanece o mesmo
void hash_apaga(thash *h){
  int pos;
  for(pos =0; pos < h->max; pos++){
    if (h->table[pos] != 0 && h->table[pos] != h->deleted){
      free((void *)h->table[pos]);
    }
  }
  free(h->table);
}

// Função para medir tempo em microssegundos
double get_time_microseconds() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1000000.0 + tv.tv_usec;
}

//CEPs
typedef struct {
  char cep_prefix[6]; // 5 dígitos + '\0'
  char cidade[100];
  char estado[3];   //sigla
} tcep_info;

// A chave será o prefixo do CEP
char * get_cep_key(void *reg) {
  return ((tcep_info *)reg)->cep_prefix;
}

// Função para alocar e preencher a nossa struct
tcep_info * aloca_cep_info(const char *cep, const char *cidade, const char *estado) {
  tcep_info *info = malloc(sizeof(tcep_info));
  if (info) {
    strncpy(info->cep_prefix, cep, 5);
    info->cep_prefix[5] = '\0';
    strncpy(info->cidade, cidade, 99);
    info->cidade[99] = '\0';
    strncpy(info->estado, estado, 2);
    info->estado[2] = '\0';
  }
  return info;
}
// Função auxiliar para remover aspas
char* remove_aspas(char* str) {
  if (str == NULL) return NULL;
  
  // Remove aspas do início
  if (str[0] == '"') {
      str++;
  }
  
  // Remove aspas do final
  int len = strlen(str);
  if (len > 0 && str[len-1] == '"') {
      str[len-1] = '\0';
  }
  
  return str;
}

// FUNÇÂO P LER O CSV ADQUADAMENTE
/*
 * Função para carregar dados de um arquivo CSV com faixas de CEP.
 * A função insere uma entrada para cada prefixo de 5 dígitos na faixa.
 */
void carregar_ceps_por_faixa(const char *filename, thash *h) {
  FILE *fp = fopen(filename, "r");
  if (!fp) {
    perror("Erro ao abrir o arquivo");
    exit(EXIT_FAILURE);
  }

  char linha[1024];
  int contador_linhas = 0;
  //int prefixos_inseridos = 0;

  // Pula a primeira linha (cabeçalho)
  fgets(linha, sizeof(linha), fp);

  while (fgets(linha, sizeof(linha), fp)) {
    contador_linhas++;

    linha[strcspn(linha, "\n")] = 0;
    char copia_linha[1024];
    strcpy(copia_linha, linha);

    char *estado = strtok(copia_linha, ",");
    char *localidade = strtok(NULL, ",");
    strtok(NULL, ","); // Ignora "Faixa de CEP"
    char *cep_inicial_str = strtok(NULL, ",");
    char *cep_final_str = strtok(NULL, ",");

    estado = remove_aspas(estado);
    localidade = remove_aspas(localidade);
    cep_inicial_str = remove_aspas(cep_inicial_str);
    cep_final_str = remove_aspas(cep_final_str);

    // DEBUG: Print da linha sendo processada
    if (contador_linhas <= 5) {
    printf("Linha %d: %s\n", contador_linhas, linha);
    printf("  Estado: %s, Localidade: %s, CEP inicial: %s, CEP final: %s\n", 
            estado ? estado : "NULL", 
            localidade ? localidade : "NULL",
            cep_inicial_str ? cep_inicial_str : "NULL",
            cep_final_str ? cep_final_str : "NULL");
    }

    // Verifica se todas as colunas necessárias foram lidas com sucesso
    if (estado && localidade && cep_inicial_str && cep_final_str) {
        
      // Converte strings de CEP para inteiros
      long cep_inicial = atol(cep_inicial_str);
      long cep_final = atol(cep_final_str);

      // Calcula os prefixos de 5 dígitos
      int prefixo_inicial = cep_inicial / 1000;
      int prefixo_final = cep_final / 1000;
      
      // Loop para inserir cada prefixo da faixa na tabela hash
      for (int p = prefixo_inicial; p <= prefixo_final; p++) {
        char prefixo_atual_str[6];
        sprintf(prefixo_atual_str, "%05d", p); // Garante 5 dígitos com zeros à esquerda

        // Verifica se este prefixo já foi inserido para evitar duplicatas
        if (hash_busca(*h, prefixo_atual_str) == NULL) {
          tcep_info *info = aloca_cep_info(prefixo_atual_str, localidade, estado);
          if (hash_insere(h, info) != EXIT_SUCCESS) {
            fprintf(stderr, "Falha ao inserir o prefixo %s. Tabela cheia?\n", prefixo_atual_str);
            free(info); // Libera memória se a inserção falhar
          }
        }
      }
    } else {
      fprintf(stderr, "AVISO: Linha %d mal formatada ou incompleta.\n", contador_linhas);
    }
  }
  printf("Carregamento concluído. %d linhas processadas.\n", contador_linhas);
  fclose(fp);
}


// --- Protótipos das funções de teste ---
void teste_overhead_insercao(tcep_info **lista_prefixos, int total_prefixos);
void teste_busca_por_ocupacao(tcep_info **lista_prefixos, int total_prefixos);

//MAIN
int main() {
  // NOME DO SEU ARQUIVO CSV
  const char *NOME_ARQUIVO = "Lista_de_CEPs.csv";
  thash h_temp;
  //tamanho grande para evitar redimensionamento nesta fase
  hash_constroi(&h_temp, 100000, get_cep_key, DOUBLE_HASHING, 0.95f);
    
  printf("Iniciando carregamento e processamento do arquivo CSV...\n");
  carregar_ceps_por_faixa(NOME_ARQUIVO, &h_temp);
  printf("Total de prefixos únicos encontrados: %d\n", h_temp.size);

  // 2. Copie os ponteiros para um array mestre
  tcep_info **lista_mestra_prefixos = malloc(sizeof(tcep_info*) * h_temp.size);
  if (!lista_mestra_prefixos) {
    perror("Falha ao alocar lista mestra");
    return EXIT_FAILURE;
  }
  
  int k = 0;
  for (int i = 0; i < h_temp.max; i++) {
    if (h_temp.table[i] != 0 && h_temp.table[i] != h_temp.deleted) {
      lista_mestra_prefixos[k++] = (tcep_info*)h_temp.table[i];
    }
  }
  free(h_temp.table);

  // --- ETAPA DE TESTES ---
  // Teste 4.2: Overhead de Inserção
  teste_overhead_insercao(lista_mestra_prefixos, k);

  // Teste 4.1: Tempo de Busca vs. Ocupação
  teste_busca_por_ocupacao(lista_mestra_prefixos, k);


  // --- LIMPEZA FINAL ---
  for(int i = 0; i < k; i++) {
      free(lista_mestra_prefixos[i]);
  }
  free(lista_mestra_prefixos);

  printf("Testes concluídos.\n");
  return 0;
}

// ---- IMPLEMENTAÇÃO DAS FUNÇÕES DE TESTE ----

// Função para o teste 4.2
void insere1000(tcep_info **lista_prefixos, int total_prefixos) {
  thash h;
  hash_constroi(&h, 1000, get_cep_key, DOUBLE_HASHING, 0.7f); //forçará resize
  for (int i = 0; i < total_prefixos; i++) {
    tcep_info* info = aloca_cep_info(lista_prefixos[i]->cep_prefix, lista_prefixos[i]->cidade, lista_prefixos[i]->estado);
    hash_insere(&h, info);
  }
  hash_apaga(&h);
}

void insere_suficiente(tcep_info **lista_prefixos, int total_prefixos) {
  thash h;
  // Cria uma tabela grande o suficiente para não precisar de resize
  int tamanho_inicial = (int)(total_prefixos / 0.6f); // Ex: para caber com 60% de ocupação
  hash_constroi(&h, tamanho_inicial, get_cep_key, DOUBLE_HASHING, 0.7f);
  for (int i = 0; i < total_prefixos; i++) {
    tcep_info* info = aloca_cep_info(lista_prefixos[i]->cep_prefix, lista_prefixos[i]->cidade, lista_prefixos[i]->estado);
    hash_insere(&h, info);
  }
  hash_apaga(&h);
}

void teste_overhead_insercao(tcep_info **lista_prefixos, int total_prefixos) {
  printf("\n--- Iniciando Teste de Overhead de Inserção ---\n");
  
  // Teste com tabela pequena (forçará resize)
  double inicio = get_time_microseconds();
  insere1000(lista_prefixos, total_prefixos);
  double fim = get_time_microseconds();
  double tempo_com_resize = (fim - inicio) / 1000.0;
  
  // Teste com tabela suficiente (sem resize)
  inicio = get_time_microseconds();
  insere_suficiente(lista_prefixos, total_prefixos);
  fim = get_time_microseconds();
  double tempo_sem_resize = (fim - inicio) / 1000.0;
  /*
  printf("\n=== TABELA DE COMPARAÇÃO - OVERHEAD DE INSERÇÃO ===\n");
  printf("+---------------------------+---------------+\n");
  printf("| Tipo de Teste             | Tempo (ms)    |\n");
  printf("+---------------------------+---------------+\n");
  printf("| Com Resize (tabela 1000)  |    %8.3f    |\n", tempo_com_resize);
  printf("| Sem Resize (tabela grande)|    %8.3f    |\n", tempo_sem_resize);
  printf("| Overhead do Resize        |    %8.3f    |\n", tempo_com_resize - tempo_sem_resize);
  printf("| Overhead Percentual       |    %7.1f%%   |\n", ((tempo_com_resize / tempo_sem_resize) - 1.0) * 100.0);
  printf("+---------------------------+---------------+\n");
  
  printf("--- Teste de Overhead Concluído ---\n");*/
}


// Funções para o teste 4.1
// funçoes de busca com HASH SIMPLES
void busca10_simples(thash* h, tcep_info** lista_a_buscar, int n) {
  for (int i = 0; i < n; i++) hash_busca(*h, lista_a_buscar[i]->cep_prefix);
}

void busca20_simples(thash* h, tcep_info** lista_a_buscar, int n) {
  for (int i = 0; i < n; i++) hash_busca(*h, lista_a_buscar[i]->cep_prefix);
}

void busca30_simples(thash* h, tcep_info** lista_a_buscar, int n) {
  for (int i = 0; i < n; i++) hash_busca(*h, lista_a_buscar[i]->cep_prefix);
}

void busca40_simples(thash* h, tcep_info** lista_a_buscar, int n) {
  for (int i = 0; i < n; i++) hash_busca(*h, lista_a_buscar[i]->cep_prefix);
}

void busca50_simples(thash* h, tcep_info** lista_a_buscar, int n) {
  for (int i = 0; i < n; i++) hash_busca(*h, lista_a_buscar[i]->cep_prefix);
}

void busca60_simples(thash* h, tcep_info** lista_a_buscar, int n) {
  for (int i = 0; i < n; i++) hash_busca(*h, lista_a_buscar[i]->cep_prefix);
}

void busca70_simples(thash* h, tcep_info** lista_a_buscar, int n) {
  for (int i = 0; i < n; i++) hash_busca(*h, lista_a_buscar[i]->cep_prefix);
}

void busca80_simples(thash* h, tcep_info** lista_a_buscar, int n) {
  for (int i = 0; i < n; i++) hash_busca(*h, lista_a_buscar[i]->cep_prefix);
}

void busca90_simples(thash* h, tcep_info** lista_a_buscar, int n) {
  for (int i = 0; i < n; i++) hash_busca(*h, lista_a_buscar[i]->cep_prefix);
}

void busca99_simples(thash* h, tcep_info** lista_a_buscar, int n) {
  for (int i = 0; i < n; i++) hash_busca(*h, lista_a_buscar[i]->cep_prefix);
}
//----------------------------------------------------------------------------------
//funçoes de busca com HASH DUPLO
void busca10_duplo(thash* h, tcep_info** lista_a_buscar, int n) {
  for (int i = 0; i < n; i++) hash_busca(*h, lista_a_buscar[i]->cep_prefix);
}

void busca20_duplo(thash* h, tcep_info** lista_a_buscar, int n) {
  for (int i = 0; i < n; i++) hash_busca(*h, lista_a_buscar[i]->cep_prefix);
}

void busca30_duplo(thash* h, tcep_info** lista_a_buscar, int n) {
  for (int i = 0; i < n; i++) hash_busca(*h, lista_a_buscar[i]->cep_prefix);
}

void busca40_duplo(thash* h, tcep_info** lista_a_buscar, int n) {
  for (int i = 0; i < n; i++) hash_busca(*h, lista_a_buscar[i]->cep_prefix);
}

void busca50_duplo(thash* h, tcep_info** lista_a_buscar, int n) {
  for (int i = 0; i < n; i++) hash_busca(*h, lista_a_buscar[i]->cep_prefix);
}

void busca60_duplo(thash* h, tcep_info** lista_a_buscar, int n) {
  for (int i = 0; i < n; i++) hash_busca(*h, lista_a_buscar[i]->cep_prefix);
}

void busca70_duplo(thash* h, tcep_info** lista_a_buscar, int n) {
  for (int i = 0; i < n; i++) hash_busca(*h, lista_a_buscar[i]->cep_prefix);
}

void busca80_duplo(thash* h, tcep_info** lista_a_buscar, int n) {
  for (int i = 0; i < n; i++) hash_busca(*h, lista_a_buscar[i]->cep_prefix);
}

void busca90_duplo(thash* h, tcep_info** lista_a_buscar, int n) {
  for (int i = 0; i < n; i++) hash_busca(*h, lista_a_buscar[i]->cep_prefix);
}

void busca99_duplo(thash* h, tcep_info** lista_a_buscar, int n) {
  for (int i = 0; i < n; i++) hash_busca(*h, lista_a_buscar[i]->cep_prefix);
}

void teste_busca_por_ocupacao(tcep_info **lista_prefixos, int total_prefixos) {
  printf("\n--- Iniciando Teste de Busca por Ocupação ---\n");
  
  int ocupacoes[] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 99};
  resultado_teste resultados[10];
  
  // Inicializa a estrutura
  for (int i = 0; i < 10; i++) {
    resultados[i].ocupacao = ocupacoes[i];
    resultados[i].tempo_linear = 0.0;
    resultados[i].tempo_duplo = 0.0;
    resultados[i].tempo_insercao_linear = 0.0;
    resultados[i].tempo_insercao_duplo = 0.0;
  }

  // Teste para HASH SIMPLES (LINEAR_PROBING)
  printf("--- Testando LINEAR_PROBING ---\n");
  thash h_simples;
  hash_constroi(&h_simples, 6100, get_cep_key, LINEAR_PROBING, 1.0f);
  
  int inseridos = 0;
  for (int i = 0; i < 10; i++) {
    int meta_insercao = (6100 * ocupacoes[i]) / 100;
    if (meta_insercao > total_prefixos) {
      meta_insercao = total_prefixos;
    }

    // Insere os elementos necessários
    for (int j = inseridos; j < meta_insercao; j++) {
      tcep_info* info = aloca_cep_info(lista_prefixos[j]->cep_prefix, 
                                       lista_prefixos[j]->cidade, 
                                       lista_prefixos[j]->estado);
      if (hash_insere(&h_simples, info) != EXIT_SUCCESS) {
        printf("Falha ao inserir elemento %d\n", j);
        free(info);
        break;
      }
    }
    inseridos = meta_insercao;

    resultados[i].ocupacao = ocupacoes[i];
    
    // Mede tempo p simples
    double inicio = get_time_microseconds();
    
    // Executa múltiplas buscas para ter uma medição mais precisa
    for (int rep = 0; rep < 1000; rep++) {
      for (int k = 0; k < inseridos && k < 100; k++) {
        hash_busca(h_simples, lista_prefixos[k]->cep_prefix);
      }
    }
    
    double fim = get_time_microseconds();
    resultados[i].tempo_linear = (fim - inicio) / 1000.0; // Converte para milissegundos
    
    printf("Ocupação %d%% - Linear: %.3f ms\n", ocupacoes[i], resultados[i].tempo_linear);
  }
  hash_apaga(&h_simples);

  // Teste para HASH DUPLO (DOUBLE_HASHING)
  printf("--- Testando DOUBLE_HASHING ---\n");
  thash h_duplo;
  hash_constroi(&h_duplo, 6100, get_cep_key, DOUBLE_HASHING, 1.0f);

  inseridos = 0;
  for (int i = 0; i < 10; i++) {
    int meta_insercao = (6100 * ocupacoes[i]) / 100;
    if (meta_insercao > total_prefixos) {
      meta_insercao = total_prefixos;
    }

    // Insere os elementos necessários
    for (int j = inseridos; j < meta_insercao; j++) {
      tcep_info* info = aloca_cep_info(lista_prefixos[j]->cep_prefix, 
                                       lista_prefixos[j]->cidade, 
                                       lista_prefixos[j]->estado);
      if (hash_insere(&h_duplo, info) != EXIT_SUCCESS) {
        printf("Falha ao inserir elemento %d no hash duplo\n", j);
        free(info);
        break;
      }
    }
    inseridos = meta_insercao;

    // Mede tempo para DOUBLE_HASHING
    double inicio = get_time_microseconds();
    
    // Executa múltiplas buscas para ter uma medição mais precisa
    for (int rep = 0; rep < 1000; rep++) {
      for (int k = 0; k < inseridos && k < 100; k++) {
        hash_busca(h_duplo, lista_prefixos[k]->cep_prefix);
      }
    }
    
    double fim = get_time_microseconds();
    resultados[i].tempo_duplo = (fim - inicio) / 1000.0; // Converte para milissegundos
    
    printf("Ocupação %d%% - Duplo: %.3f ms\n", ocupacoes[i], resultados[i].tempo_duplo);
  }
  hash_apaga(&h_duplo);
  teste_insercao_por_ocupacao(lista_prefixos, total_prefixos, resultados);

  // Imprime tabelas de comparação
  printf("\n=== TABELA DE COMPARAÇÃO - TEMPO DE BUSCA ===\n");
  printf("+-----------+---------------+---------------+---------------+\n");
  printf("| Ocupação  | Linear (ms)   | Duplo (ms)    | Diferença (%) |\n");
  printf("+-----------+---------------+---------------+---------------+\n");
  
  for (int i = 0; i < 10; i++) {
    double diferenca = ((resultados[i].tempo_duplo - resultados[i].tempo_linear) / resultados[i].tempo_linear) * 100.0;
    printf("|    %2d%%    |    %8.3f    |    %8.3f    |    %+7.1f    |\n", 
           resultados[i].ocupacao, 
           resultados[i].tempo_linear, 
           resultados[i].tempo_duplo, 
           diferenca);
  }
  printf("+-----------+---------------+---------------+---------------+\n");

  printf("\n=== TABELA DE COMPARAÇÃO - TEMPO DE INSERÇÃO ===\n");
  printf("+-----------+---------------+---------------+---------------+\n");
  printf("| Ocupação  | Linear (ms)   | Duplo (ms)    | Diferença (%) |\n");
  printf("+-----------+---------------+---------------+---------------+\n");
  
  for (int i = 0; i < 10; i++) {
    double diferenca = ((resultados[i].tempo_insercao_duplo - resultados[i].tempo_insercao_linear) / resultados[i].tempo_insercao_linear) * 100.0;
    printf("|    %2d%%    |    %8.3f    |    %8.3f    |    %+7.1f    |\n", 
           resultados[i].ocupacao, 
           resultados[i].tempo_insercao_linear, 
           resultados[i].tempo_insercao_duplo, 
           diferenca);
  }
  printf("+-----------+---------------+---------------+---------------+\n");

  // Salva os resultados completos
  salvar_resultados_csv(resultados, 10, "resultados_completos.csv");
  
  printf("--- Teste de Busca por Ocupação Concluído ---\n");
}

void teste_insercao_por_ocupacao(tcep_info **lista_prefixos, int total_prefixos, resultado_teste *resultados) {
  printf("\n--- Iniciando Teste de Inserção por Ocupação ---\n");
  
  int ocupacoes[] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 99};
  
  // Teste para HASH SIMPLES (LINEAR_PROBING)
  printf("--- Testando Inserção LINEAR_PROBING ---\n");
  for (int i = 0; i < 10; i++) {
    thash h_simples;
    hash_constroi(&h_simples, 6100, get_cep_key, LINEAR_PROBING, 1.0f);
    
    int meta_insercao = (6100 * ocupacoes[i]) / 100;
    if (meta_insercao > total_prefixos) {
      meta_insercao = total_prefixos;
    }

    // Mede tempo de inserção para LINEAR_PROBING
    double inicio = get_time_microseconds();
    
    for (int j = 0; j < meta_insercao; j++) {
      tcep_info* info = aloca_cep_info(lista_prefixos[j]->cep_prefix, 
                                       lista_prefixos[j]->cidade, 
                                       lista_prefixos[j]->estado);
      if (hash_insere(&h_simples, info) != EXIT_SUCCESS) {
        printf("Falha ao inserir elemento %d na inserção linear\n", j);
        free(info);
        break;
      }
    }
    
    double fim = get_time_microseconds();
    resultados[i].tempo_insercao_linear = (fim - inicio) / 1000.0; // Converte para milissegundos
    
    printf("Ocupação %d%% - Inserção Linear: %.3f ms\n", ocupacoes[i], resultados[i].tempo_insercao_linear);
    hash_apaga(&h_simples);
  }

  // Teste para HASH DUPLO (DOUBLE_HASHING)
  printf("--- Testando Inserção DOUBLE_HASHING ---\n");
  for (int i = 0; i < 10; i++) {
    thash h_duplo;
    hash_constroi(&h_duplo, 6100, get_cep_key, DOUBLE_HASHING, 1.0f);
    
    int meta_insercao = (6100 * ocupacoes[i]) / 100;
    if (meta_insercao > total_prefixos) {
      meta_insercao = total_prefixos;
    }

    // Mede tempo de inserção para DOUBLE_HASHING
    double inicio = get_time_microseconds();
    
    for (int j = 0; j < meta_insercao; j++) {
      tcep_info* info = aloca_cep_info(lista_prefixos[j]->cep_prefix, 
                                       lista_prefixos[j]->cidade, 
                                       lista_prefixos[j]->estado);
      if (hash_insere(&h_duplo, info) != EXIT_SUCCESS) {
        printf("Falha ao inserir elemento %d na inserção dupla\n", j);
        free(info);
        break;
      }
    }
    
    double fim = get_time_microseconds();
    resultados[i].tempo_insercao_duplo = (fim - inicio) / 1000.0; // Converte para milissegundos
    
    printf("Ocupação %d%% - Inserção Duplo: %.3f ms\n", ocupacoes[i], resultados[i].tempo_insercao_duplo);
    hash_apaga(&h_duplo);
  }
  printf("--- Teste de Inserção por Ocupação Concluído ---\n");
}

void salvar_resultados_csv(resultado_teste *resultados, int num_resultados, const char *filename) {
  FILE *fp = fopen(filename, "w");
  if (!fp) {
    perror("Erro ao criar arquivo CSV");
    return;
  }
  
  // Cabeçalho do CSV
  fprintf(fp, "Tipo,Ocupacao,Tempo_Linear_ms,Tempo_Duplo_ms\n");
  
  // Dados de BUSCA
  for (int i = 0; i < num_resultados; i++) {
    fprintf(fp, "Busca,%d,%.3f,%.3f\n", 
            resultados[i].ocupacao, 
            resultados[i].tempo_linear, 
            resultados[i].tempo_duplo);
  }
  
  // Dados de INSERÇÃO
  for (int i = 0; i < num_resultados; i++) {
    fprintf(fp, "Insercao,%d,%.3f,%.3f\n", 
            resultados[i].ocupacao, 
            resultados[i].tempo_insercao_linear, 
            resultados[i].tempo_insercao_duplo);
  }
  
  fclose(fp);
  printf("Resultados salvos em: %s\n", filename);
}