//******************************************************************
//
// Copyright 2015 Intel Mobile Communications GmbH All Rights Reserved.
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
#define _POSIX_C_SOURCE 200112L
#include <string.h>

#include "srmutility.h"
#include "srmresourcestrings.h"
#include "logger.h"
#include "oic_malloc.h"
#include "base64.h"

#define TAG  "SRM-UTILITY"

void ParseQueryIterInit(const unsigned char * query, OicParseQueryIter_t * parseIter)
{
    OC_LOG(INFO, TAG, "Initializing coap iterator");
    if ((NULL == query) || (NULL == parseIter))
    {
        return;
    }

    parseIter->attrPos = NULL;
    parseIter->attrLen = 0;
    parseIter->valPos = NULL;
    parseIter->valLen = 0;
    coap_parse_iterator_init((unsigned char *)query, strlen((char *)query),
                             (unsigned char *)OIC_SEC_REST_QUERY_SEPARATOR,
                             (unsigned char *) "", 0, &parseIter->pi);
}

OicParseQueryIter_t * GetNextQuery(OicParseQueryIter_t * parseIter)
{
    OC_LOG(INFO, TAG, "Getting Next Query");
    if (NULL == parseIter)
    {
        return NULL;
    }

    unsigned char * qrySeg = NULL;
    char * delimPos;

    // Get the next query. Querys are separated by OIC_SEC_REST_QUERY_SEPARATOR.
    qrySeg = coap_parse_next(&parseIter->pi);

    if (qrySeg)
    {
        delimPos = strchr((char *)qrySeg, OIC_SEC_REST_QUERY_DELIMETER);
        if (delimPos)
        {
            parseIter->attrPos = parseIter->pi.pos;
            parseIter->attrLen = (unsigned char *)delimPos - parseIter->pi.pos;
            parseIter->valPos  = (unsigned char *)delimPos + 1;
            parseIter->valLen  = &qrySeg[parseIter->pi.segment_length] - parseIter->valPos;
            return parseIter;
        }
    }
    return NULL;
}

// TODO This functionality is replicated in all SVR's and therefore we need
// to encapsulate it in a common method. However, this may not be the right
// file for this method.
OCStackResult AddUuidArray(const cJSON* jsonRoot, const char* arrayItem,
                           size_t *numUuids, OicUuid_t** uuids)
{
    size_t idxx = 0;
    cJSON* jsonObj = cJSON_GetObjectItem((cJSON *)jsonRoot, arrayItem);
    VERIFY_NON_NULL(TAG, jsonObj, ERROR);
    VERIFY_SUCCESS(TAG, cJSON_Array == jsonObj->type, ERROR);

    *numUuids = cJSON_GetArraySize(jsonObj);
    VERIFY_SUCCESS(TAG, *numUuids > 0, ERROR);
    *uuids = (OicUuid_t*)OICCalloc(*numUuids, sizeof(OicUuid_t));
    VERIFY_NON_NULL(TAG, *uuids, ERROR);

    do
    {
        unsigned char base64Buff[sizeof(((OicUuid_t*)0)->id)] = {};
        uint32_t outLen = 0;
        B64Result b64Ret = B64_OK;

        cJSON *jsonOwnr = cJSON_GetArrayItem(jsonObj, idxx);
        VERIFY_NON_NULL(TAG, jsonOwnr, ERROR);
        VERIFY_SUCCESS(TAG, cJSON_String == jsonOwnr->type, ERROR);

        outLen = 0;
        b64Ret = b64Decode(jsonOwnr->valuestring, strlen(jsonOwnr->valuestring), base64Buff,
                sizeof(base64Buff), &outLen);

        VERIFY_SUCCESS(TAG, (b64Ret == B64_OK && outLen <= sizeof((*uuids)[idxx].id)),
                ERROR);
        memcpy((*uuids)[idxx].id, base64Buff, outLen);
    } while ( ++idxx < *numUuids);

    return OC_STACK_OK;

exit:
    return OC_STACK_ERROR;

}
