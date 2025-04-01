/**
 * @file http_client.h
 * @brief Functions for HTTPS communication with server
 */
#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <stdbool.h>

bool send_parking_update(const char* spot_id, bool is_taken);

#endif /* HTTP_CLIENT_H */