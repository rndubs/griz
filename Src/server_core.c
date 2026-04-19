/*
 * server_core.c - JSON envelope helpers for griz-server.
 *
 * See server_core.h for the request/response contract. This file is
 * linked only into the server binary (guarded by GRIZ_SERVER_BUILD)
 * so cJSON is not pulled into the batch/interactive binaries.
 */

#ifdef GRIZ_SERVER_BUILD

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "server_core.h"

/* Heuristic: try JSON parsing only when the line looks like an object.
 * parse_command accepts lots of syntax, and feeding every raw command
 * through cJSON_Parse() would be wasteful (and would produce spurious
 * parse errors logged via cJSON_GetErrorPtr).
 */
static int
looks_like_json_object( const char *line )
{
    while ( *line == ' ' || *line == '\t' )
        line++;
    return *line == '{';
}

int
server_parse_request( const char *line, ServerRequest *req )
{
    cJSON *root;
    cJSON *cmd_item;
    cJSON *id_item;

    req->cmd  = NULL;
    req->id   = NULL;
    req->json = NULL;

    if ( !looks_like_json_object( line ) )
    {
        req->cmd = line;
        return 0;
    }

    root = cJSON_Parse( line );
    if ( root == NULL )
    {
        server_emit_error( NULL, "invalid_request",
                           "malformed JSON request" );
        return -1;
    }

    cmd_item = cJSON_GetObjectItemCaseSensitive( root, "cmd" );
    if ( !cJSON_IsString( cmd_item ) || cmd_item->valuestring == NULL )
    {
        id_item = cJSON_GetObjectItemCaseSensitive( root, "id" );
        server_emit_error(
            cJSON_IsString( id_item ) ? id_item->valuestring : NULL,
            "invalid_request",
            "request missing string 'cmd' field" );
        cJSON_Delete( root );
        return -1;
    }

    id_item = cJSON_GetObjectItemCaseSensitive( root, "id" );

    req->cmd  = cmd_item->valuestring;
    req->id   = ( cJSON_IsString( id_item ) ) ? id_item->valuestring : NULL;
    req->json = root;
    return 0;
}

void
server_request_free( ServerRequest *req )
{
    if ( req == NULL || req->json == NULL )
        return;
    cJSON_Delete( (cJSON *) req->json );
    req->json = NULL;
    req->cmd  = NULL;
    req->id   = NULL;
}

static void
add_id_field( cJSON *obj, const char *id )
{
    if ( id != NULL )
        cJSON_AddStringToObject( obj, "id", id );
    else
        cJSON_AddNullToObject( obj, "id" );
}

void
server_emit_response( const char *id, int status_ok,
                      const char *stdout_s, const char *stderr_s )
{
    cJSON *root;
    char  *rendered;

    root = cJSON_CreateObject();
    if ( root == NULL )
        return;

    cJSON_AddStringToObject( root, "type",   "response" );
    add_id_field( root, id );
    cJSON_AddStringToObject( root, "status", status_ok ? "ok" : "error" );
    cJSON_AddStringToObject( root, "stdout", stdout_s ? stdout_s : "" );
    cJSON_AddStringToObject( root, "stderr", stderr_s ? stderr_s : "" );
    cJSON_AddNullToObject(   root, "data" );

    rendered = cJSON_PrintUnformatted( root );
    if ( rendered != NULL )
    {
        fputs( rendered, stdout );
        fputc( '\n', stdout );
        fflush( stdout );
        free( rendered );
    }

    cJSON_Delete( root );
}

void
server_emit_error( const char *id, const char *code, const char *message )
{
    cJSON *root;
    cJSON *err;
    char  *rendered;

    root = cJSON_CreateObject();
    if ( root == NULL )
        return;

    cJSON_AddStringToObject( root, "type",   "response" );
    add_id_field( root, id );
    cJSON_AddStringToObject( root, "status", "error" );

    err = cJSON_CreateObject();
    cJSON_AddStringToObject( err, "code",    code    ? code    : "internal_error" );
    cJSON_AddStringToObject( err, "message", message ? message : "" );
    cJSON_AddItemToObject( root, "error", err );

    rendered = cJSON_PrintUnformatted( root );
    if ( rendered != NULL )
    {
        fputs( rendered, stdout );
        fputc( '\n', stdout );
        fflush( stdout );
        free( rendered );
    }

    cJSON_Delete( root );
}

#endif /* GRIZ_SERVER_BUILD */
