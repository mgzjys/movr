#include <R.h>
#include <Rdefines.h>
#include <glib.h>

#include "order.h"

/**
 * Compress individual's movement history
 * 
 * The compression indicates removing duplicate records at the location with
 * two consecutive records and compressing the duplicate into a single session.
 * 
 * @param loc character vector
 * @param time real vector of timestamps in seconds
 * @param gap a length one vector to separate two sessions when the location
 *  does not change
 * @return a data frame of compressed movement data
 */
SEXP
_compress_mov(SEXP loc, SEXP time, SEXP gap) {
  // convert R objects to C data structure
  const size_t NLEN = LENGTH(time);
  double *time_ = REAL(time);
  size_t *ordered, i, j, idx;
  double last_time, cur_time;
  char *last_loc, *cur_loc;
  SEXP out_df, loc_v, stime_v, etime_v;
  double gap_ = asReal(gap);
  
  // sort observations in time
  ordered = malloc(sizeof(size_t) * NLEN);
  order(time_, NLEN, sizeof(double), cmpDouble, ordered);
  
  // compress movement
  PROTECT(out_df = NEW_LIST(3));
  PROTECT(loc_v = NEW_CHARACTER(NLEN));
  PROTECT(stime_v = NEW_NUMERIC(NLEN));
  PROTECT(etime_v = NEW_NUMERIC(NLEN));
  
  for ( i = 0, j = 0; i < NLEN; i++) {
    idx = ordered[i];
    cur_loc = CHAR(STRING_ELT(loc, idx));
    cur_time = time_[idx];
    
    if ( i == 0 ){
      SET_STRING_ELT(loc_v, j, mkChar(cur_loc));
      REAL(stime_v)[j] = cur_time;
      REAL(etime_v)[j] = cur_time;
      j++;
    } else {
      if (strcmp(cur_loc, last_loc) == 0 && cur_time - last_time <= gap_) {
        // the same session, update last session
        REAL(etime_v)[j-1] = cur_time;
      } else {
        // a new session
        SET_STRING_ELT(loc_v, j, mkChar(cur_loc));
        REAL(stime_v)[j] = cur_time;
        REAL(etime_v)[j] = cur_time;
        j++;
      }
    }

    last_loc = cur_loc;
    last_time = cur_time;
  }

  // a R data frame to store results
  SET_VECTOR_ELT(out_df, 0, loc_v);
  SET_VECTOR_ELT(out_df, 1, stime_v);
  SET_VECTOR_ELT(out_df, 2, etime_v);
  
  UNPROTECT(4);
  
  return out_df;
}

/**
 * Calculate the flow statistic for each link (of a location pair)
 * 
 * @param loc character vector
 * @param stime real vector
 * @param etime real vector
 * @param gap length one vector of real number
 */
SEXP
_flow_stat(SEXP loc, SEXP stime, SEXP etime, SEXP gap) {
  double *stime_ = REAL(stime);
  double *etime_ = REAL(etime);
  double gap_ = asReal(gap);
  char *last_loc, *cur_loc;
  double last_et;
  int i;
  
  GHashTable * stat = g_hash_table_new(g_str_hash, g_int_equal);
  last_loc = CHAR(STRING_ELT(loc, 0));
  last_et = etime_[0];
  
  for ( i = 1; i < length(loc); i++ ){
    if ( stime_[i] - last_et <= gap_ ) {
      // assemble new link name   
      char *link;
      link = malloc(sizeof(char) * (strlen(last_loc) + strlen(cur_loc) + 3));
      cur_loc = CHAR(STRING_ELT(loc, i));
      sprintf(link, "%s->%s", last_loc, cur_loc);
      
      // update flow stat
      if ( ! g_hash_table_contains(stat, link) ) {
        g_hash_table_insert(stat, link, 0);
      }
      g_hash_table_insert(stat, link,
        GPOINTER_TO_INT(g_hash_table_lookup(stat, link)) + 1 );
    }
    
    last_loc = CHAR(STRING_ELT(loc, i));
    last_et = etime_[i];
  }
  
  // convert flow stat in hash table to R data frame
  SEXP out, edges, flows;
  const int PAIR_NUM = g_hash_table_size(stat);
  GHashTableIter iter;
  gpointer key, value;
  i = 0;
  
  PROTECT(out = NEW_LIST(2));
  PROTECT(edges = NEW_CHARACTER(PAIR_NUM));
  PROTECT(flows = NEW_INTEGER(PAIR_NUM));
  
  g_hash_table_iter_init(&iter, stat);
  while (g_hash_table_iter_next (&iter, &key, &value)) {
    SET_STRING_ELT(edges, i, mkChar((char*)key));
    INTEGER(flows)[i] = GPOINTER_TO_INT(value);
    i++;
  }
  
  SET_VECTOR_ELT(out, 0, edges);
  SET_VECTOR_ELT(out, 1, flows);
  
  UNPROTECT(3);
  
  g_hash_table_destroy(stat);
  
  return out;
}