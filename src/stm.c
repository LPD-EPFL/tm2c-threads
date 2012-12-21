#include "stm.h"

void
tx_metadata_node_print(stm_tx_node_t * stm_tx_node) 
{
  printf("TXs Statistics for node --------------------------------------\n");
  printf("Starts      \t: %u\n", stm_tx_node->tx_starts);
  printf("Commits     \t: %u\n", stm_tx_node->tx_commited);
  printf("Aborts      \t: %u\n", stm_tx_node->tx_aborted);
  printf("Max Retries \t: %u\n", stm_tx_node->max_retries);
  printf("Aborts WAR  \t: %u\n", stm_tx_node->aborts_war);
  printf("Aborts RAW  \t: %u\n", stm_tx_node->aborts_raw);
  printf("Aborts WAW  \t: %u\n", stm_tx_node->aborts_waw);
  printf("--------------------------------------------------------------\n");
  fflush(stdout);
}

void
tx_metadata_print(stm_tx_t* stm_tx) 
{
  printf("TX Statistics ------------------------------------------------\n");
  printf("Retries     \t: %u\n", stm_tx->retries);
  printf("Aborts      \t: %u\n", stm_tx->aborts);
  printf("Aborts WAR  \t: %u\n", stm_tx->aborts_war);
  printf("Aborts RAW  \t: %u\n", stm_tx->aborts_raw);
  printf("Aborts WAW  \t: %u\n", stm_tx->aborts_waw);
  printf("--------------------------------------------------------------\n");
  fflush(stdout);
}

stm_tx_node_t*
tx_metadata_node_new() 
{
  stm_tx_node_t *stm_tx_node_temp = (stm_tx_node_t *) malloc(sizeof (stm_tx_node_t));
  if (stm_tx_node_temp == NULL) 
    {
      printf("malloc stm_tx_node @ tx_metadata_node_new");
      return NULL;
    }

  stm_tx_node_temp->tx_starts = 0;
  stm_tx_node_temp->tx_commited = 0;
  stm_tx_node_temp->tx_aborted = 0;
  stm_tx_node_temp->max_retries = 0;
  stm_tx_node_temp->aborts_war = 0;
  stm_tx_node_temp->aborts_raw = 0;
  stm_tx_node_temp->aborts_waw = 0;

#if defined(FAIRCM)
  stm_tx_node_temp->tx_duration = 1;
#endif

  return stm_tx_node_temp;
}

stm_tx_t* 
tx_metadata_new() 
  {
    stm_tx_t *stm_tx_temp = (stm_tx_t *) malloc(sizeof(stm_tx_t));
    if (stm_tx_temp == NULL) 
      {
	printf("malloc stm_tx @ tx_metadata_new");
	return NULL;
      }

#if !defined(PGAS) 
    stm_tx_temp->write_set = write_set_new();
#endif
    stm_tx_temp->mem_info = mem_info_new();

    stm_tx_temp->retries = 0;
    stm_tx_temp->aborts = 0;
    stm_tx_temp->aborts_war = 0;
    stm_tx_temp->aborts_raw = 0;
    stm_tx_temp->aborts_waw = 0;
    /* stm_tx_temp->max_retries = 0; */

#if defined(FAIRCM) || defined(GREEDY)
    stm_tx_temp->start_ts = 0;
#endif

    return stm_tx_temp;
  }

void
tx_metadata_free(stm_tx_t **stm_tx) 
{
#if !defined(PGAS)
  write_set_free((*stm_tx)->write_set);
#endif
  mem_info_free((*stm_tx)->mem_info);
  free((*stm_tx));
  *stm_tx = NULL;
}
