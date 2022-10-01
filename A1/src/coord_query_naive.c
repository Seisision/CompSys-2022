#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <stdint.h>
#include <errno.h>
#include <assert.h>

#include "record.h"
#include "coord_query.h"

struct naive_data {
  struct record *rs;
  int n;
};

struct naive_data* mk_naive(struct record* rs, int n) {
  assert(0);
  // TODO
}

void free_naive(struct naive_data* data) {
  free(data);
  return;
  // TODO
}

const struct record* lookup_naive(struct naive_data *data, double lon, double lat) {
  double distance = 0;
  struct record *retVal;
  for(int i = 0; i < (data->n); i++) {
      double lonDistance = (data->rs[i].lon)-lon;
      double latDistance = (data->rs[i].lat)-lat;
      double currentDistance = pow(lonDistance, 2.) + pow(latDistance, 2.);
      if( currentDistance < distance ) {
        distance = currentDistance;
          retVal = &(data->rs[i]);
      }
  }
  return retVal;
  // TODO
}

int main(int argc, char** argv) {
  return coord_query_loop(argc, argv,
                          (mk_index_fn)mk_naive,
                          (free_index_fn)free_naive,
                          (lookup_fn)lookup_naive);
}
