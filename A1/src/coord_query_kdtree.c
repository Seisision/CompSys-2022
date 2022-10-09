#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <stdint.h>
#include <errno.h>
#include <assert.h>
#include <float.h>
#include <math.h>

#include "record.h"
#include "coord_query.h"

struct kd_record {
    double lon;
    double lat;
    const struct record *record;
};

struct node { 
    struct kd_record *k_record;
    // axis = 0 if lon and axis = 1 if lat
    int axis;
    struct node *left;
    struct node *right;
};

struct kd_data {
  struct kd_record *krs;
  struct node *root;
  int n;
};

int compareRecordLon(const void *a, const void *b) {
    double _a = ((struct kd_record*)a)->lon;
    double _b = ((struct kd_record*)b)->lon;

    if (_a > _b) {
        return 1;
    } else if (_a < _b) {
        return -1;
    } else {
        return 0;
    }
}

int compareRecordLat(const void *a, const void *b) {
    double _a = ((struct kd_record*)a)->lat;
    double _b = ((struct kd_record*)b)->lat;

    if (_a > _b) {
        return 1;
    } else if (_a < _b) {
        return -1;
    } else {
        return 0;
    }
}

struct node* build_tree(struct kd_record *krs, int n, int axis) {
    // sort for lon if axis = 0 and lat if axis = 1
    if (axis == 0) {
        qsort(krs, n, sizeof(struct kd_record), compareRecordLon);
    } else {
        qsort(krs, n, sizeof(struct kd_record), compareRecordLat);
    }
    // records are now sorted so median is middle index
    int median = n/2;

    //  allocate node
    struct node *kd_node = (struct node*)malloc(sizeof(struct node));
    // set node fields
    kd_node->k_record = &(krs[median]);
    kd_node->axis = axis;
    // if less than 2 nodes we have no left subtree
    if (n >= 2) {
        kd_node->left = build_tree(krs, median, (axis + 1) % 2);
    } else {
        kd_node->left = NULL;
    }
    // if less than 3 nodes we have no right subtree
    if (n >= 3) {
        kd_node->right = build_tree(&(krs[median+1]), n-median-1, (axis + 1) % 2);
    } else {
        kd_node->right = NULL;
    }

    return kd_node;
}



struct kd_data* mk_kdtree(struct record* rs, int n) {
   struct kd_record *krs = (struct kd_record*)malloc(sizeof(struct kd_record)*n);

    for(int i = 0; i < n; i++) {
        krs[i].lon =  rs[i].lon;
        krs[i].lat =  rs[i].lat;
        krs[i].record = &(rs[i]);
    }

    struct kd_data *data = (struct kd_data*)malloc(sizeof(struct kd_data));

    data->n = n;
    data->krs = krs;
    data->root = build_tree(krs, n, 0);

    return data;
}

void free_node(struct node *nd) {
    if (nd->left != NULL) {
        free_node(nd->left);
    }
    if (nd->right != NULL) {
        free_node(nd->right);
    }
    free(nd);
}

void free_kd(struct kd_data* data) {
    free_node(data->root);
    free(data->krs);
    free(data);  
}

void lookup_closest(struct kd_record **closest, double lon, double lat, struct node *nd) {
    if (nd == NULL) {
        return;
    }
    
    // calculate distance to current closest
    double closest_dist_sq = DBL_MAX;   
    struct kd_record *current_closest = *closest;
    if (current_closest != NULL)
    {
        closest_dist_sq = pow(lon-current_closest->lon ,2.) + pow(lat-current_closest->lat, 2.);
    }

    // set node as closest if closest
    double dist_sq = pow(lon - nd->k_record->lon, 2.) + pow(lat - nd->k_record->lat, 2.);
    if (dist_sq < closest_dist_sq) {
        // update pointer to closest
        (*closest) = nd->k_record;
        closest_dist_sq = dist_sq;
    }

    // calculate distance and radius
    double diff = 0;
    if (nd->axis == 0) {
        diff = nd->k_record->lon - lon;
    } else {
        diff = nd->k_record->lat - lat;
    }
    double radius = sqrt(closest_dist_sq);
    
    // find subtree to search    
    if (diff >= 0 || radius > fabs(diff)) {
        lookup_closest(closest, lon, lat, nd->left);
    }
    if (diff <= 0 || radius > fabs(diff)) {
        lookup_closest(closest, lon, lat, nd->right);
    }
}

const struct record* lookup_coord_kdtree(struct kd_data *data, double lon, double lat) {
    // intialize search pointer
    struct kd_record *closest = NULL;
    lookup_closest(&closest, lon, lat, data->root);
    if (closest == NULL) {
        return NULL;
    }
    return closest->record;
}



int main(int argc, char** argv) {
  return coord_query_loop(argc, argv,
                          (mk_index_fn)mk_kdtree,
                          (free_index_fn)free_kd,
                          (lookup_fn)lookup_coord_kdtree);
}
