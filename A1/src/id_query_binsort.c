#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <stdint.h>
#include <errno.h>
#include <assert.h>

#include "record.h"
#include "id_query.h"

struct indexed_data {
    struct index_record *irs;
    int n;
};

struct index_record {
    int64_t osm_id;
    const struct record *record;
};

int compareRecord(const void *a, const void *b) {
    return (((struct index_record*)a)->osm_id - ((struct index_record*)b)->osm_id);
}

struct indexed_data* mk_sort_indexed(struct record* rs, int n) {
    struct index_record *irs = (struct index_record*)malloc(sizeof(struct index_record)*n);

    for(int i = 0; i < n; i++) {
        irs[i].osm_id =  rs[i].osm_id;
        irs[i].record = &(rs[i]);
    }

    qsort(irs, n, sizeof(struct index_record), compareRecord);

    struct indexed_data *data = (struct indexed_data*)malloc(sizeof(struct indexed_data));

    data->n = n;
    data->irs = irs;

    return data;
}

void free_sort_indexed(struct indexed_data* data) {
    free(data->irs);
    free(data);
}

const struct record* lookup_bin(struct indexed_data *data,int64_t needle) {
    // TODO binary search for needle
}


int main(int argc, char** argv) {
  return id_query_loop(argc, argv,
                    (mk_index_fn)mk_sort_indexed,
                    (free_index_fn)free_sort_indexed,
                    (lookup_fn)lookup_bin);
}
