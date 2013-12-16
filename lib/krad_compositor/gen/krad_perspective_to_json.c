#include "krad_perspective_to_json.h"

int kr_pos_to_json(char *json, void *st, int32_t max) {
  int res;
  res = 0;
  struct kr_pos *actual;

  if ((json == NULL) || (st == NULL) || (max < 1)) {
    return -1;
  }

  actual = (struct kr_pos*)st;

  res += snprintf(&json[res],max,"{");
  res += snprintf(&json[res],max,"\"x\" : %u,",actual->x);
  res += snprintf(&json[res],max,"\"y\" : %u",actual->y);
  res += snprintf(&json[res],max,"}");

  return res;
}

int kr_perspective_view_to_json(char *json, void *st, int32_t max) {
  uber_St uber;
  int res;
  res = 0;
  struct kr_perspective_view *actual;

  if ((json == NULL) || (st == NULL) || (max < 1)) {
    return -1;
  }

  actual = (struct kr_perspective_view*)st;

  res += snprintf(&json[res],max,"{");
  res += snprintf(&json[res],max,"\"top_left\": ");
  uber.actual = &(actual->top_left);
  uber.type = JSON_KR_POS;
  res += info_pack_to_json(&json[res],&uber,max-res);
  res += snprintf(&json[res],max,",");
  res += snprintf(&json[res],max,"\"top_right\": ");
  uber.actual = &(actual->top_right);
  uber.type = JSON_KR_POS;
  res += info_pack_to_json(&json[res],&uber,max-res);
  res += snprintf(&json[res],max,",");
  res += snprintf(&json[res],max,"\"bottom_left\": ");
  uber.actual = &(actual->bottom_left);
  uber.type = JSON_KR_POS;
  res += info_pack_to_json(&json[res],&uber,max-res);
  res += snprintf(&json[res],max,",");
  res += snprintf(&json[res],max,"\"bottom_right\": ");
  uber.actual = &(actual->bottom_right);
  uber.type = JSON_KR_POS;
  res += info_pack_to_json(&json[res],&uber,max-res);
  res += snprintf(&json[res],max,"}");

  return res;
}

int kr_perspective_to_json(char *json, void *st, int32_t max) {
  uber_St uber;
  int res;
  res = 0;
  struct kr_perspective *actual;

  if ((json == NULL) || (st == NULL) || (max < 1)) {
    return -1;
  }

  actual = (struct kr_perspective*)st;

  res += snprintf(&json[res],max,"{");
  res += snprintf(&json[res],max,"\"width\" : %u,",actual->width);
  res += snprintf(&json[res],max,"\"height\" : %u,",actual->height);
  res += snprintf(&json[res],max,"\"view\": ");
  uber.actual = &(actual->view);
  uber.type = JSON_KR_PERSPECTIVE_VIEW;
  res += info_pack_to_json(&json[res],&uber,max-res);
  res += snprintf(&json[res],max,"}");

  return res;
}

