//******************************************************************
//
// Copyright 2015 Samsung Electronics All Rights Reserved.
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

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "utlist.h"
#include "cJSON.h"
#include "base64.h"
#include "cainterface.h"
#include "ocstack.h"
#include "oic_malloc.h"
#include "oic_string.h"
#include "ocpayload.h"
#include "ocpayloadcbor.h"
#include "payload_logging.h"
#include "secureresourcemanager.h"
#include "srmresourcestrings.h"
#include "srmutility.h"
#include "aclresource.h"
#include "pstatresource.h"
#include "doxmresource.h"
#include "amaclresource.h"
#include "credresource.h"
#include "security_internals.h"
#include "ocresourcehandler.h"

#define TAG  "OIC_JSON2CBOR"
#define MAX_RANGE   SIZE_MAX
//SVR database buffer block size

#define DB_FILE_SIZE_BLOCK 1023

static OicSecPstat_t* JSONToPstatBin(const char * jsonStr);
static OicSecDoxm_t* JSONToDoxmBin(const char * jsonStr);
static OicSecAcl_t *JSONToAclBin(const char * jsonStr);
static OicSecAmacl_t* JSONToAmaclBin(const char * jsonStr);
static OicSecCred_t* JSONToCredBin(const char * jsonStr);
static OCDeviceProperties* JSONToDPBin(const char *jsonStr);

static size_t GetJSONFileSize(const char *jsonFileName)
{
    size_t size = 0;
    size_t bytesRead  = 0;
    char buffer[DB_FILE_SIZE_BLOCK];
    FILE* fp = fopen(jsonFileName, "r");
    if (fp)
    {
        do
        {
            bytesRead = fread(buffer, 1, DB_FILE_SIZE_BLOCK, fp);
            if (bytesRead >=(MAX_RANGE - size))
            {
                fclose(fp);
                return 0;
            }
            size += bytesRead;
        } while (bytesRead > 0);
        fclose(fp);
    }
    return size;
}

static void ConvertJsonToCBOR(const char *jsonFileName, const char *cborFileName)
{
    char *jsonStr = NULL;
    FILE *fp = NULL;
    FILE *fp1 = NULL;
    uint8_t *aclCbor = NULL;
    uint8_t *pstatCbor = NULL;
    uint8_t *doxmCbor = NULL;
    uint8_t *amaclCbor = NULL;
    uint8_t *credCbor = NULL;
    uint8_t *dpCbor = NULL;
    cJSON *jsonRoot = NULL;
    OCStackResult ret = OC_STACK_ERROR;
    OCDeviceProperties *deviceProps = NULL;

    size_t size = GetJSONFileSize(jsonFileName);
    if (0 == size)
    {
        OIC_LOG (ERROR, TAG, "Failed converting to JSON");
        return;
    }

    jsonStr = (char *)OICMalloc(size + 1);
    VERIFY_NOT_NULL(TAG, jsonStr, FATAL);

    fp = fopen(jsonFileName, "r");
    if (fp)
    {
        size_t bytesRead = fread(jsonStr, 1, size, fp);
        jsonStr[bytesRead] = '\0';

        OIC_LOG_V(DEBUG, TAG, "Read %" PRIuPTR " bytes", bytesRead);
        fclose(fp);
        fp = NULL;
    }
    else
    {
        OIC_LOG (ERROR, TAG, "Unable to open JSON file!!");
        goto exit;
    }

    jsonRoot = cJSON_Parse(jsonStr);

    cJSON *value = cJSON_GetObjectItem(jsonRoot, OIC_JSON_ACL_NAME);
    //printf("ACL json : \n%s\n", cJSON_PrintUnformatted(value));
    size_t aclCborSize = 0;
    if (NULL != value)
    {
        OicSecAcl_t *acl = JSONToAclBin(jsonStr);
        VERIFY_NOT_NULL(TAG, acl, FATAL);
        ret = AclToCBORPayload(acl, &aclCbor, &aclCborSize);
        if(OC_STACK_OK != ret)
        {
            OIC_LOG (ERROR, TAG, "Failed converting Acl to Cbor Payload");
            DeleteACLList(acl);
            goto exit;
        }
        printf("ACL Cbor Size: %" PRIuPTR "\n", aclCborSize);
        DeleteACLList(acl);
    }

    value = cJSON_GetObjectItem(jsonRoot, OIC_JSON_PSTAT_NAME);
    size_t pstatCborSize = 0;
    if (NULL != value)
    {
        OicSecPstat_t *pstat = JSONToPstatBin(jsonStr);
        VERIFY_NOT_NULL(TAG, pstat, FATAL);
        ret = PstatToCBORPayload(pstat, &pstatCbor, &pstatCborSize, false);
        if(OC_STACK_OK != ret)
        {
            OIC_LOG (ERROR, TAG, "Failed converting Pstat to Cbor Payload");
            DeletePstatBinData(pstat);
            goto exit;
        }
        printf("PSTAT Cbor Size: %" PRIuPTR "\n", pstatCborSize);
        DeletePstatBinData(pstat);
    }
    value = cJSON_GetObjectItem(jsonRoot, OIC_JSON_DOXM_NAME);
    size_t doxmCborSize = 0;
    if (NULL != value)
    {
        OicSecDoxm_t *doxm = JSONToDoxmBin(jsonStr);
        VERIFY_NOT_NULL(TAG, doxm, FATAL);
        ret = DoxmToCBORPayload(doxm, &doxmCbor, &doxmCborSize, false);
        if(OC_STACK_OK != ret)
        {
            OIC_LOG (ERROR, TAG, "Failed converting Doxm to Cbor Payload");
            DeleteDoxmBinData(doxm);
            goto exit;
        }
        printf("DOXM Cbor Size: %" PRIuPTR "\n", doxmCborSize);
        DeleteDoxmBinData(doxm);
    }
    value = cJSON_GetObjectItem(jsonRoot, OIC_JSON_AMACL_NAME);
    size_t amaclCborSize = 0;
    if (NULL != value)
    {
        OicSecAmacl_t *amacl = JSONToAmaclBin(jsonStr);
        VERIFY_NOT_NULL(TAG, amacl, FATAL);
        ret = AmaclToCBORPayload(amacl, &amaclCbor, &amaclCborSize);
        if(OC_STACK_OK != ret)
        {
            OIC_LOG (ERROR, TAG, "Failed converting Amacl to Cbor Payload");
            DeleteAmaclList(amacl);
            goto exit;
        }
        printf("AMACL Cbor Size: %" PRIuPTR "\n", amaclCborSize);
        DeleteAmaclList(amacl);
    }
    value = cJSON_GetObjectItem(jsonRoot, OIC_JSON_CRED_NAME);
    //printf("CRED json : \n%s\n", cJSON_PrintUnformatted(value));
    size_t credCborSize = 0;
    int secureFlag = 0;
    if (NULL != value)
    {
        OicSecCred_t *cred = JSONToCredBin(jsonStr);
        VERIFY_NOT_NULL(TAG, cred, FATAL);
        ret = CredToCBORPayload(cred, &credCbor, &credCborSize, secureFlag);
        if(OC_STACK_OK != ret)
        {
            OIC_LOG (ERROR, TAG, "Failed converting Cred to Cbor Payload");
            DeleteCredList(cred);
            goto exit;
        }
        printf("CRED Cbor Size: %" PRIuPTR "\n", credCborSize);
        DeleteCredList(cred);
    }
    value = cJSON_GetObjectItem(jsonRoot, OC_JSON_DEVICE_PROPS_NAME);
    size_t dpCborSize = 0;
    if (NULL != value)
    {
        deviceProps = JSONToDPBin(jsonStr);
        VERIFY_NOT_NULL(TAG, deviceProps, FATAL);
        ret = DevicePropertiesToCBORPayload(deviceProps, &dpCbor, &dpCborSize);
        if (OC_STACK_OK != ret)
        {
            OIC_LOG(ERROR, TAG, "Failed converting OCDeviceProperties to Cbor Payload");
            goto exit;
        }
        printf("Device Properties Cbor Size: %" PRIuPTR "\n", dpCborSize);
    }

    CborEncoder encoder;
    size_t cborSize = aclCborSize + pstatCborSize + doxmCborSize + credCborSize + amaclCborSize + dpCborSize;

    printf("Total Cbor Size : %" PRIuPTR "\n", cborSize);
    cborSize += 255; // buffer margin for adding map and byte string
    uint8_t *outPayload = (uint8_t *)OICCalloc(1, cborSize);
    VERIFY_NOT_NULL(TAG, outPayload, ERROR);
    cbor_encoder_init(&encoder, outPayload, cborSize, 0);
    CborEncoder map;
    CborError cborEncoderResult = cbor_encoder_create_map(&encoder, &map, CborIndefiniteLength);
    VERIFY_CBOR_SUCCESS(TAG, cborEncoderResult, "Failed Creating Main Map.");
    if (aclCborSize > 0)
    {
        cborEncoderResult = cbor_encode_text_string(&map, OIC_JSON_ACL_NAME, strlen(OIC_JSON_ACL_NAME));
        VERIFY_CBOR_SUCCESS(TAG, cborEncoderResult, "Failed Adding ACL Name.");
        cborEncoderResult = cbor_encode_byte_string(&map, aclCbor, aclCborSize);
        VERIFY_CBOR_SUCCESS(TAG, cborEncoderResult, "Failed Adding ACL Value.");
    }

    if (pstatCborSize > 0)
    {
        cborEncoderResult = cbor_encode_text_string(&map, OIC_JSON_PSTAT_NAME, strlen(OIC_JSON_PSTAT_NAME));
        VERIFY_CBOR_SUCCESS(TAG, cborEncoderResult, "Failed Adding PSTAT Name.");
        cborEncoderResult = cbor_encode_byte_string(&map, pstatCbor, pstatCborSize);
        VERIFY_CBOR_SUCCESS(TAG, cborEncoderResult, "Failed Adding PSTAT Value.");
    }
    if (doxmCborSize > 0)
    {
        cborEncoderResult = cbor_encode_text_string(&map, OIC_JSON_DOXM_NAME, strlen(OIC_JSON_DOXM_NAME));
        VERIFY_CBOR_SUCCESS(TAG, cborEncoderResult, "Failed Adding DOXM Name.");
        cborEncoderResult = cbor_encode_byte_string(&map, doxmCbor, doxmCborSize);
        VERIFY_CBOR_SUCCESS(TAG, cborEncoderResult, "Failed Adding DOXM Value.");
    }
    if (amaclCborSize > 0)
    {
        cborEncoderResult = cbor_encode_text_string(&map, OIC_JSON_AMACL_NAME, strlen(OIC_JSON_AMACL_NAME));
        VERIFY_CBOR_SUCCESS(TAG, cborEncoderResult, "Failed Adding AMACL Name.");
        cborEncoderResult = cbor_encode_byte_string(&map, amaclCbor, amaclCborSize);
        VERIFY_CBOR_SUCCESS(TAG, cborEncoderResult, "Failed Adding AMACL Value.");
    }
    if (credCborSize > 0)
    {
        cborEncoderResult = cbor_encode_text_string(&map, OIC_JSON_CRED_NAME, strlen(OIC_JSON_CRED_NAME));
        VERIFY_CBOR_SUCCESS(TAG, cborEncoderResult, "Failed Adding CRED Name.");
        cborEncoderResult = cbor_encode_byte_string(&map, credCbor, credCborSize);
        VERIFY_CBOR_SUCCESS(TAG, cborEncoderResult, "Failed Adding CRED Value.");
    }
    if (dpCborSize > 0)
    {
        cborEncoderResult = cbor_encode_text_string(&map, OC_JSON_DEVICE_PROPS_NAME, strlen(OC_JSON_DEVICE_PROPS_NAME));
        VERIFY_CBOR_SUCCESS(TAG, cborEncoderResult, "Failed Adding Device Properties Name.");
        cborEncoderResult = cbor_encode_byte_string(&map, dpCbor, dpCborSize);
        VERIFY_CBOR_SUCCESS(TAG, cborEncoderResult, "Failed Adding Device Properties Value.");
    }

    cborEncoderResult = cbor_encoder_close_container(&encoder, &map);
    VERIFY_CBOR_SUCCESS(TAG, cborEncoderResult, "Failed Closing Container.");

    size_t s = cbor_encoder_get_buffer_size(&encoder, outPayload);
    OIC_LOG_V(DEBUG, TAG, "Payload size %" PRIuPTR, s);

    fp1 = fopen(cborFileName, "w");
    if (fp1)
    {
        size_t bytesWritten = fwrite(outPayload, 1, s, fp1);
        if (bytesWritten == s)
        {
            OIC_LOG_V(DEBUG, TAG, "Written %" PRIuPTR " bytes", bytesWritten);
        }
        else
        {
            OIC_LOG_V(ERROR, TAG, "Failed writing %" PRIuPTR " bytes", s);
        }
        fclose(fp1);
        fp1 = NULL;
    }
exit:

    cJSON_Delete(jsonRoot);
    OICFree(aclCbor);
    OICFree(doxmCbor);
    OICFree(pstatCbor);
    OICFree(amaclCbor);
    OICFree(credCbor);
    OICFree(jsonStr);
    OICFree(dpCbor);
    CleanUpDeviceProperties(&deviceProps);
    return;
}

OicSecAcl_t* JSONToAclBin(const char * jsonStr)
{
    OCStackResult ret = OC_STACK_ERROR;
    OicSecAcl_t * headAcl = (OicSecAcl_t*)OICCalloc(1, sizeof(OicSecAcl_t));
    cJSON *jsonRoot = NULL;

    VERIFY_NOT_NULL(TAG, jsonStr, ERROR);

    jsonRoot = cJSON_Parse(jsonStr);
    VERIFY_NOT_NULL(TAG, jsonRoot, ERROR);

    cJSON *jsonAclMap = cJSON_GetObjectItem(jsonRoot, OIC_JSON_ACL_NAME);
    VERIFY_NOT_NULL(TAG, jsonAclMap, ERROR);

    cJSON *jsonAclObj = NULL;

    // aclist
    jsonAclObj = cJSON_GetObjectItem(jsonAclMap, OIC_JSON_ACLIST_NAME);
    VERIFY_NOT_NULL(TAG, jsonAclObj, ERROR);

    // aclist-aces
    cJSON *jsonAclArray = NULL;
    jsonAclArray = cJSON_GetObjectItem(jsonAclObj, OIC_JSON_ACES_NAME);
    VERIFY_NOT_NULL(TAG, jsonAclArray, ERROR);

    if (cJSON_Array == jsonAclArray->type)
    {

        int numAcl = cJSON_GetArraySize(jsonAclArray);
        int idx = 0;

        VERIFY_SUCCESS(TAG, numAcl > 0, INFO);
        do
        {
            cJSON *jsonAcl = cJSON_GetArrayItem(jsonAclArray, idx);
            VERIFY_NOT_NULL(TAG, jsonAcl, ERROR);

            OicSecAce_t *ace = (OicSecAce_t*)OICCalloc(1, sizeof(OicSecAce_t));
            VERIFY_NOT_NULL(TAG, ace, ERROR);
            LL_APPEND(headAcl->aces, ace);

            cJSON *jsonObj = NULL;
            jsonObj = cJSON_GetObjectItem(jsonAcl, OIC_JSON_SUBJECTID_NAME);
            VERIFY_NOT_NULL(TAG, jsonObj, ERROR);
            VERIFY_SUCCESS(TAG, cJSON_String == jsonObj->type, ERROR);
            if(strcmp(jsonObj->valuestring, WILDCARD_RESOURCE_URI) == 0)
            {
                ace->subjectuuid.id[0] = '*';
            }
            else
            {
                ret = ConvertStrToUuid(jsonObj->valuestring, &ace->subjectuuid);
                VERIFY_SUCCESS(TAG, OC_STACK_OK == ret, ERROR);
            }
            // Resources -- Mandatory
            jsonObj = cJSON_GetObjectItem(jsonAcl, OIC_JSON_RESOURCES_NAME);
            VERIFY_NOT_NULL(TAG, jsonObj, ERROR);
            VERIFY_SUCCESS(TAG, cJSON_Array == jsonObj->type, ERROR);

            size_t resourcesLen = cJSON_GetArraySize(jsonObj);
            VERIFY_SUCCESS(TAG, resourcesLen > 0, ERROR);

            for(size_t idxx = 0; idxx < resourcesLen; idxx++)
            {
                OicSecRsrc_t* rsrc = (OicSecRsrc_t*)OICCalloc(1, sizeof(OicSecRsrc_t));
                VERIFY_NOT_NULL(TAG, rsrc, ERROR);

// Needs to be removed once IOT-1746 is resolved.
#ifdef _MSC_VER
#pragma warning(suppress : 4267)
                cJSON *jsonRsrc = cJSON_GetArrayItem(jsonObj, idxx);

#else
                cJSON *jsonRsrc = cJSON_GetArrayItem(jsonObj, idxx);

#endif
                VERIFY_NOT_NULL(TAG, jsonRsrc, ERROR);

                //href
                cJSON *jsonRsrcObj = cJSON_GetObjectItem(jsonRsrc, OIC_JSON_HREF_NAME);
                VERIFY_NOT_NULL(TAG, jsonRsrcObj, ERROR);
                VERIFY_SUCCESS(TAG, cJSON_String == jsonRsrcObj->type, ERROR);

                rsrc->href = OICStrdup(jsonRsrcObj->valuestring);
                VERIFY_NOT_NULL(TAG, (rsrc->href), ERROR);

                //rel
                jsonRsrcObj = cJSON_GetObjectItem(jsonRsrc, OIC_JSON_REL_NAME);
                if(jsonRsrcObj)
                {
                    rsrc->rel = OICStrdup(jsonRsrcObj->valuestring);
                    VERIFY_NOT_NULL(TAG, (rsrc->rel), ERROR);
                }

                //rt
                jsonRsrcObj = cJSON_GetObjectItem(jsonRsrc, OIC_JSON_RT_NAME);
                if(jsonRsrcObj && cJSON_Array == jsonRsrcObj->type)
                {
                    rsrc->typeLen = cJSON_GetArraySize(jsonRsrcObj);
                    VERIFY_SUCCESS(TAG, (0 < rsrc->typeLen), ERROR);
                    rsrc->types = (char**)OICCalloc(rsrc->typeLen, sizeof(char*));
                    VERIFY_NOT_NULL(TAG, (rsrc->types), ERROR);
                    for(size_t i = 0; i < rsrc->typeLen; i++)
                    {
// Needs to be removed once IOT-1746 is resolved.
#ifdef _MSC_VER
#pragma warning(suppress : 4267)
                        cJSON *jsonRsrcType = cJSON_GetArrayItem(jsonRsrcObj, i);

#else
                        cJSON *jsonRsrcType = cJSON_GetArrayItem(jsonRsrcObj, i);

#endif
                        VERIFY_NOT_NULL(TAG, jsonRsrcType, ERROR);
                        rsrc->types[i] = OICStrdup(jsonRsrcType->valuestring);
                        VERIFY_NOT_NULL(TAG, (rsrc->types[i]), ERROR);
                    }
                }

                //if
                jsonRsrcObj = cJSON_GetObjectItem(jsonRsrc, OIC_JSON_IF_NAME);
                if(jsonRsrcObj && cJSON_Array == jsonRsrcObj->type)
                {
                    rsrc->interfaceLen = cJSON_GetArraySize(jsonRsrcObj);
                    VERIFY_SUCCESS(TAG, (0 < rsrc->interfaceLen), ERROR);
                    rsrc->interfaces = (char**)OICCalloc(rsrc->interfaceLen, sizeof(char*));
                    VERIFY_NOT_NULL(TAG, (rsrc->interfaces), ERROR);
                    for(size_t i = 0; i < rsrc->interfaceLen; i++)
                    {
// Needs to be removed once IOT-1746 is resolved.
#ifdef _MSC_VER
#pragma warning(suppress : 4267)
                        cJSON *jsonInterface = cJSON_GetArrayItem(jsonRsrcObj, i);

#else
                        cJSON *jsonInterface = cJSON_GetArrayItem(jsonRsrcObj, i);

#endif
                        VERIFY_NOT_NULL(TAG, jsonInterface, ERROR);
                        rsrc->interfaces[i] = OICStrdup(jsonInterface->valuestring);
                        VERIFY_NOT_NULL(TAG, (rsrc->interfaces[i]), ERROR);
                    }
                }

                LL_APPEND(ace->resources, rsrc);
            }

            // Permissions -- Mandatory
            jsonObj = cJSON_GetObjectItem(jsonAcl, OIC_JSON_PERMISSION_NAME);
            VERIFY_NOT_NULL(TAG, jsonObj, ERROR);
            VERIFY_SUCCESS(TAG, cJSON_Number == jsonObj->type, ERROR);
            VERIFY_SUCCESS(TAG, jsonObj->valueint <= UINT16_MAX, ERROR);
            ace->permission = (uint16_t)jsonObj->valueint;

            //Validity -- Not Mandatory
            cJSON *jsonValidityObj = cJSON_GetObjectItem(jsonAcl, OIC_JSON_VALIDITY_NAME);
            if(jsonValidityObj)
            {
                VERIFY_SUCCESS(TAG, cJSON_Array == jsonValidityObj->type, ERROR);
                size_t validityLen = cJSON_GetArraySize(jsonValidityObj);
                VERIFY_SUCCESS(TAG, (0 < validityLen), ERROR);

                cJSON *jsonValidity = NULL;
                for(size_t i = 0; i < validityLen; i++)
                {
// Needs to be removed once IOT-1746 is resolved.
#ifdef _MSC_VER
#pragma warning(suppress : 4267)
                    jsonValidity = cJSON_GetArrayItem(jsonValidityObj, i);

#else
                    jsonValidity = cJSON_GetArrayItem(jsonValidityObj, i);

#endif
                    VERIFY_NOT_NULL(TAG, jsonValidity, ERROR);
                    VERIFY_SUCCESS(TAG, (jsonValidity->type == cJSON_Array), ERROR);

                    OicSecValidity_t* validity = (OicSecValidity_t*)OICCalloc(1, sizeof(OicSecValidity_t));
                    VERIFY_NOT_NULL(TAG, validity, ERROR);
                    LL_APPEND(ace->validities, validity);

                    //Period
                    cJSON* jsonPeriod = cJSON_GetArrayItem(jsonValidity, 0);
                    if(jsonPeriod)
                    {
                        VERIFY_SUCCESS(TAG, (cJSON_String == jsonPeriod->type), ERROR);

                        validity->period = OICStrdup(jsonPeriod->valuestring);
                        VERIFY_NOT_NULL(TAG, validity->period, ERROR);
                    }

                    //Recurrence
                    cJSON* jsonRecurObj = cJSON_GetArrayItem(jsonValidity, 1);
                    if(jsonRecurObj)
                    {
                        VERIFY_SUCCESS(TAG, (cJSON_Array == jsonRecurObj->type), ERROR);
                        validity->recurrenceLen = cJSON_GetArraySize(jsonRecurObj);
                        VERIFY_SUCCESS(TAG, (0 < validity->recurrenceLen), ERROR);

                        validity->recurrences = (char**)OICCalloc(validity->recurrenceLen, sizeof(char*));
                        VERIFY_NOT_NULL(TAG, validity->recurrences, ERROR);

                        cJSON *jsonRecur = NULL;
                        for(size_t j = 0; j < validity->recurrenceLen; j++)
                        {
// Needs to be removed once IOT-1746 is resolved.
#ifdef _MSC_VER
#pragma warning(suppress : 4267)
                            jsonRecur = cJSON_GetArrayItem(jsonRecurObj, j);

#else
                            jsonRecur = cJSON_GetArrayItem(jsonRecurObj, j);

#endif
                            VERIFY_NOT_NULL(TAG, jsonRecur, ERROR);
                            validity->recurrences[j] = OICStrdup(jsonRecur->valuestring);
                            VERIFY_NOT_NULL(TAG, validity->recurrences[j], ERROR);
                        }
                    }
                }
            }
        } while( ++idx < numAcl);
    }


    // rownerid
    jsonAclObj = cJSON_GetObjectItem(jsonAclMap, OIC_JSON_ROWNERID_NAME);
    VERIFY_NOT_NULL(TAG, jsonAclObj, ERROR);
    VERIFY_SUCCESS(TAG, cJSON_String == jsonAclObj->type, ERROR);
    ret = ConvertStrToUuid(jsonAclObj->valuestring, &headAcl->rownerID);
    VERIFY_SUCCESS(TAG, OC_STACK_OK == ret, ERROR);

    ret = OC_STACK_OK;

exit:
    cJSON_Delete(jsonRoot);
    if (OC_STACK_OK != ret)
    {
        DeleteACLList(headAcl);
        headAcl = NULL;
    }
    return headAcl;
}

OicSecDoxm_t* JSONToDoxmBin(const char * jsonStr)
{
    printf("IN JSONToDoxmBin\n");
    if (NULL == jsonStr)
    {
        return NULL;
    }

    OCStackResult ret = OC_STACK_ERROR;
    OicSecDoxm_t *doxm =  NULL;
    cJSON *jsonDoxm = NULL;
    cJSON *jsonObj = NULL;

    size_t jsonObjLen = 0;

    cJSON *jsonRoot = cJSON_Parse(jsonStr);
    VERIFY_NOT_NULL(TAG, jsonRoot, ERROR);

    jsonDoxm = cJSON_GetObjectItem(jsonRoot, OIC_JSON_DOXM_NAME);
    VERIFY_NOT_NULL(TAG, jsonDoxm, ERROR);

    doxm = (OicSecDoxm_t *)OICCalloc(1, sizeof(OicSecDoxm_t));
    VERIFY_NOT_NULL(TAG, doxm, ERROR);

    //OxmType -- not Mandatory
    jsonObj = cJSON_GetObjectItem(jsonDoxm, OIC_JSON_OXM_TYPE_NAME);
    if ((jsonObj) && (cJSON_Array == jsonObj->type))
    {
        doxm->oxmTypeLen = cJSON_GetArraySize(jsonObj);
        VERIFY_SUCCESS(TAG, doxm->oxmTypeLen > 0, ERROR);

        doxm->oxmType = (OicUrn_t *)OICCalloc(doxm->oxmTypeLen, sizeof(char *));
        VERIFY_NOT_NULL(TAG, (doxm->oxmType), ERROR);

        for (size_t i  = 0; i < doxm->oxmTypeLen ; i++)
        {
// Needs to be removed once IOT-1746 is resolved.
#ifdef _MSC_VER
#pragma warning(suppress : 4267)
            cJSON *jsonOxmTy = cJSON_GetArrayItem(jsonObj, i);

#else
            cJSON *jsonOxmTy = cJSON_GetArrayItem(jsonObj, i);

#endif
            VERIFY_NOT_NULL(TAG, jsonOxmTy, ERROR);

            jsonObjLen = strlen(jsonOxmTy->valuestring) + 1;
            doxm->oxmType[i] = (char*)OICMalloc(jsonObjLen);
            VERIFY_NOT_NULL(TAG, doxm->oxmType[i], ERROR);
            strncpy((char *)doxm->oxmType[i], (char *)jsonOxmTy->valuestring, jsonObjLen);
        }
    }

    //Oxm -- not Mandatory
    jsonObj = cJSON_GetObjectItem(jsonDoxm, OIC_JSON_OXMS_NAME);
    if (jsonObj && cJSON_Array == jsonObj->type)
    {
        doxm->oxmLen = cJSON_GetArraySize(jsonObj);
        VERIFY_SUCCESS(TAG, doxm->oxmLen > 0, ERROR);

        doxm->oxm = (OicSecOxm_t*)OICCalloc(doxm->oxmLen, sizeof(OicSecOxm_t));
        VERIFY_NOT_NULL(TAG, doxm->oxm, ERROR);

        for (size_t i  = 0; i < doxm->oxmLen ; i++)
        {
// Needs to be removed once IOT-1746 is resolved.
#ifdef _MSC_VER
#pragma warning(suppress : 4267)
            cJSON *jsonOxm = cJSON_GetArrayItem(jsonObj, i);

#else
            cJSON *jsonOxm = cJSON_GetArrayItem(jsonObj, i);

#endif
            VERIFY_NOT_NULL(TAG, jsonOxm, ERROR);
            doxm->oxm[i] = (OicSecOxm_t)jsonOxm->valueint;
        }
    }

    //OxmSel -- Mandatory
// Needs to be removed once IOT-1746 is resolved.
#ifdef _MSC_VER
#pragma warning(suppress : 4267)
    jsonObj = cJSON_GetObjectItem(jsonDoxm, OIC_JSON_OXM_SEL_NAME);

#else
    jsonObj = cJSON_GetObjectItem(jsonDoxm, OIC_JSON_OXM_SEL_NAME);

#endif
    if (jsonObj)
    {
        VERIFY_SUCCESS(TAG, cJSON_Number == jsonObj->type, ERROR);
        doxm->oxmSel = (OicSecOxm_t)jsonObj->valueint;
    }

    //sct -- Mandatory
    jsonObj = cJSON_GetObjectItem(jsonDoxm, OIC_JSON_SUPPORTED_CRED_TYPE_NAME);
    if (jsonObj)
    {
        VERIFY_SUCCESS(TAG, cJSON_Number == jsonObj->type, ERROR);
        doxm->sct = (OicSecCredType_t)jsonObj->valueint;
    }

    //Owned -- Mandatory
    jsonObj = cJSON_GetObjectItem(jsonDoxm, OIC_JSON_OWNED_NAME);
    if (jsonObj)
    {
        VERIFY_SUCCESS(TAG, (cJSON_True == jsonObj->type || cJSON_False == jsonObj->type), ERROR);
        doxm->owned = jsonObj->valueint;
    }

#ifdef MULTIPLE_OWNER
    //mom -- Not Mandatory
    jsonObj = cJSON_GetObjectItem(jsonDoxm, OIC_JSON_MOM_NAME);
    if (jsonObj)
    {
        VERIFY_SUCCESS(TAG, (cJSON_Number == jsonObj->type), ERROR);
        doxm->mom = (OicSecMom_t*)OICCalloc(1, sizeof(OicSecMom_t));
        VERIFY_NOT_NULL(TAG, doxm->mom, ERROR);
        doxm->mom->mode = (OicSecMomType_t)jsonObj->valueint;
    }
#endif //MULTIPLE_OWNER

    //DeviceId -- Mandatory
    jsonObj = cJSON_GetObjectItem(jsonDoxm, OIC_JSON_DEVICE_ID_NAME);
    if (jsonObj)
    {
        VERIFY_SUCCESS(TAG, cJSON_String == jsonObj->type, ERROR);
        if (cJSON_String == jsonObj->type)
        {
            //Check for empty string, in case DeviceId field has not been set yet
            if (jsonObj->valuestring[0])
            {
                ret = ConvertStrToUuid(jsonObj->valuestring, &doxm->deviceID);
                VERIFY_SUCCESS(TAG, OC_STACK_OK == ret, ERROR);
            }
        }
    }

    //rowner -- Mandatory
    jsonObj = cJSON_GetObjectItem(jsonDoxm, OIC_JSON_ROWNERID_NAME);
    if (true == doxm->owned)
    {
        VERIFY_NOT_NULL(TAG, jsonObj, ERROR);
    }
    if (jsonObj)
    {
        ret = ConvertStrToUuid(jsonObj->valuestring, &doxm->rownerID);
        VERIFY_SUCCESS(TAG, OC_STACK_OK == ret, ERROR);
    }

    //Owner -- will be empty when device status is unowned.
    jsonObj = cJSON_GetObjectItem(jsonDoxm, OIC_JSON_DEVOWNERID_NAME);
    if (true == doxm->owned)
    {
        VERIFY_NOT_NULL(TAG, jsonObj, ERROR);
    }
    if (jsonObj)
    {
        ret = ConvertStrToUuid(jsonObj->valuestring, &doxm->owner);
                VERIFY_SUCCESS(TAG, OC_STACK_OK == ret, ERROR);
    }

    ret = OC_STACK_OK;

exit:
    cJSON_Delete(jsonRoot);
    if (OC_STACK_OK != ret)
    {
        DeleteDoxmBinData(doxm);
        doxm = NULL;
    }
    printf("OUT %s: %s\n", __func__, (doxm != NULL) ? "success" : "failure");
    return doxm;
}

OicSecPstat_t* JSONToPstatBin(const char * jsonStr)
{
    printf("IN JSONToPstatBin\n");
    if(NULL == jsonStr)
    {
        return NULL;
    }

    OCStackResult ret = OC_STACK_ERROR;
    OicSecPstat_t *pstat = NULL;
    cJSON *jsonPstat = NULL;
    cJSON *jsonObj = NULL;

    cJSON *jsonRoot = cJSON_Parse(jsonStr);
    VERIFY_NOT_NULL(TAG, jsonRoot, INFO);

    jsonPstat = cJSON_GetObjectItem(jsonRoot, OIC_JSON_PSTAT_NAME);
    VERIFY_NOT_NULL(TAG, jsonPstat, INFO);

    pstat = (OicSecPstat_t*)OICCalloc(1, sizeof(OicSecPstat_t));
    VERIFY_NOT_NULL(TAG, pstat, INFO);
    jsonObj = cJSON_GetObjectItem(jsonPstat, OIC_JSON_ISOP_NAME);
    VERIFY_NOT_NULL(TAG, jsonObj, ERROR);
    VERIFY_SUCCESS(TAG, (cJSON_True == jsonObj->type || cJSON_False == jsonObj->type) , ERROR);
    pstat->isOp = jsonObj->valueint;

    jsonObj = cJSON_GetObjectItem(jsonPstat, OIC_JSON_ROWNERID_NAME);
    VERIFY_NOT_NULL(TAG, jsonObj, ERROR);
    VERIFY_SUCCESS(TAG, cJSON_String == jsonObj->type, ERROR);
    ret = ConvertStrToUuid(jsonObj->valuestring, &pstat->rownerID);
    VERIFY_SUCCESS(TAG, OC_STACK_OK == ret, ERROR);

    jsonObj = cJSON_GetObjectItem(jsonPstat, OIC_JSON_CM_NAME);
    VERIFY_NOT_NULL(TAG, jsonObj, ERROR);
    VERIFY_SUCCESS(TAG, cJSON_Number == jsonObj->type, ERROR);
    pstat->cm  = (OicSecDpm_t)jsonObj->valueint;

    jsonObj = cJSON_GetObjectItem(jsonPstat, OIC_JSON_TM_NAME);
    VERIFY_NOT_NULL(TAG, jsonObj, ERROR);
    VERIFY_SUCCESS(TAG, cJSON_Number == jsonObj->type, ERROR);
    pstat->tm  = (OicSecDpm_t)jsonObj->valueint;

    jsonObj = cJSON_GetObjectItem(jsonPstat, OIC_JSON_OM_NAME);
    VERIFY_NOT_NULL(TAG, jsonObj, ERROR);
    VERIFY_SUCCESS(TAG, cJSON_Number == jsonObj->type, ERROR);
    pstat->om  = (OicSecDpom_t)jsonObj->valueint;

    jsonObj = cJSON_GetObjectItem(jsonPstat, OIC_JSON_SM_NAME);
    VERIFY_NOT_NULL(TAG, jsonObj, ERROR);
    VERIFY_SUCCESS(TAG, cJSON_Number == jsonObj->type, ERROR);
    pstat->smLen = 1;
    pstat->sm = (OicSecDpom_t*)OICCalloc(pstat->smLen, sizeof(OicSecDpom_t));
    pstat->sm[0] = (OicSecDpom_t)jsonObj->valueint;

    ret = OC_STACK_OK;

exit:
    cJSON_Delete(jsonRoot);
    if (OC_STACK_OK != ret)
    {
        DeletePstatBinData(pstat);
        pstat = NULL;
    }
    printf("OUT %s: %s\n", __func__, (pstat != NULL) ? "success" : "failure");
    return pstat;
}

static OicEncodingType_t GetEncodingTypeFromStr(const char* encodingType)
{
    if (strcmp(OIC_SEC_ENCODING_RAW, encodingType) == 0)
    {
        return OIC_ENCODING_RAW;
    }
    if (strcmp(OIC_SEC_ENCODING_BASE64, encodingType) == 0)
    {
        return OIC_ENCODING_BASE64;
    }
    if (strcmp(OIC_SEC_ENCODING_PEM, encodingType) == 0)
    {
        return OIC_ENCODING_PEM;
    }
    if (strcmp(OIC_SEC_ENCODING_DER, encodingType) == 0)
    {
        return OIC_ENCODING_DER;
    }
    OIC_LOG(WARNING, TAG, "Unknow encoding type dectected!");
    OIC_LOG(WARNING, TAG, "json2cbor will use \"oic.sec.encoding.raw\" as default encoding type.");
    return OIC_ENCODING_RAW;
}

OicSecCred_t * JSONToCredBin(const char * jsonStr)
{
    if (NULL == jsonStr)
    {
        OIC_LOG(ERROR, TAG,"JSONToCredBin jsonStr in NULL");
        return NULL;
    }

    OicSecCred_t *headCred = (OicSecCred_t*)OICCalloc(1, sizeof(OicSecCred_t));
    OCStackResult ret = OC_STACK_ERROR;
    cJSON *jsonRoot = NULL;
    VERIFY_NOT_NULL(TAG, headCred, ERROR);

    jsonRoot = cJSON_Parse(jsonStr);
    VERIFY_NOT_NULL(TAG, jsonRoot, ERROR);

    cJSON *jsonCredMap = cJSON_GetObjectItem(jsonRoot, OIC_JSON_CRED_NAME);
    VERIFY_NOT_NULL(TAG, jsonCredMap, ERROR);

    // creds
    cJSON *jsonCredArray = NULL;
    jsonCredArray = cJSON_GetObjectItem(jsonCredMap, OIC_JSON_CREDS_NAME);
    VERIFY_NOT_NULL(TAG, jsonCredArray, ERROR);

    if (cJSON_Array == jsonCredArray->type)
    {
        int numCred = cJSON_GetArraySize(jsonCredArray);
        VERIFY_SUCCESS(TAG, numCred > 0, ERROR);
        int idx = 0;
        do
        {
            cJSON *jsonCred = cJSON_GetArrayItem(jsonCredArray, idx);
            VERIFY_NOT_NULL(TAG, jsonCred, ERROR);

            OicSecCred_t *cred = NULL;
            if(idx == 0)
            {
                cred = headCred;
            }
            else
            {
                cred = (OicSecCred_t*)OICCalloc(1, sizeof(OicSecCred_t));
                OicSecCred_t *temp = headCred;
                while (temp->next)
                {
                    temp = temp->next;
                }
                temp->next = cred;
            }
            VERIFY_NOT_NULL(TAG, cred, ERROR);

            size_t jsonObjLen = 0;
            cJSON *jsonObj = NULL;

            //CredId -- Mandatory
            jsonObj = cJSON_GetObjectItem(jsonCred, OIC_JSON_CREDID_NAME);
            if(jsonObj)
            {
                VERIFY_SUCCESS(TAG, cJSON_Number == jsonObj->type, ERROR);
                VERIFY_SUCCESS(TAG, jsonObj->valueint <= UINT16_MAX, ERROR);
                cred->credId = (uint16_t)jsonObj->valueint;
            }

            //subject -- Mandatory
            jsonObj = cJSON_GetObjectItem(jsonCred, OIC_JSON_SUBJECTID_NAME);
            VERIFY_NOT_NULL(TAG, jsonObj, ERROR);
            VERIFY_SUCCESS(TAG, cJSON_String == jsonObj->type, ERROR);
            if(strcmp(jsonObj->valuestring, WILDCARD_RESOURCE_URI) == 0)
            {
                cred->subject.id[0] = '*';
            }
            else
            {
                ret = ConvertStrToUuid(jsonObj->valuestring, &cred->subject);
                VERIFY_SUCCESS(TAG, OC_STACK_OK == ret, ERROR);
            }

            //CredType -- Mandatory
            jsonObj = cJSON_GetObjectItem(jsonCred, OIC_JSON_CREDTYPE_NAME);
            VERIFY_NOT_NULL(TAG, jsonObj, ERROR);
            VERIFY_SUCCESS(TAG, cJSON_Number == jsonObj->type, ERROR);
            cred->credType = (OicSecCredType_t)jsonObj->valueint;
            //PrivateData is mandatory for some of the credential types listed below.
            jsonObj = cJSON_GetObjectItem(jsonCred, OIC_JSON_PRIVATEDATA_NAME);

            if (NULL != jsonObj)
            {
                cJSON *jsonPriv = cJSON_GetObjectItem(jsonObj, OIC_JSON_DATA_NAME);
                VERIFY_NOT_NULL(TAG, jsonPriv, ERROR);
                jsonObjLen = strlen(jsonPriv->valuestring);
                cred->privateData.data = (uint8_t *)OICCalloc(1, jsonObjLen);
                VERIFY_NOT_NULL(TAG, (cred->privateData.data), ERROR);
                memcpy(cred->privateData.data, jsonPriv->valuestring, jsonObjLen);
                cred->privateData.len = jsonObjLen;

                cJSON *jsonEncoding = cJSON_GetObjectItem(jsonObj, OIC_JSON_ENCODING_NAME);
                VERIFY_NOT_NULL(TAG, jsonEncoding, ERROR);
                cred->privateData.encoding = GetEncodingTypeFromStr(jsonEncoding->valuestring);
            }
#if defined(__WITH_DTLS__) || defined(__WITH_TLS__)
            //PublicData is mandatory only for SIGNED_ASYMMETRIC_KEY credentials type.
            jsonObj = cJSON_GetObjectItem(jsonCred, OIC_JSON_PUBLICDATA_NAME);

            if (NULL != jsonObj)
            {
                cJSON *jsonPub = cJSON_GetObjectItem(jsonObj, OIC_JSON_DATA_NAME);
                VERIFY_NOT_NULL(TAG, jsonPub, ERROR);
                jsonObjLen = strlen(jsonPub->valuestring);
                cred->publicData.data = (uint8_t *)OICCalloc(1, jsonObjLen);
                VERIFY_NOT_NULL(TAG, (cred->publicData.data), ERROR);
                memcpy(cred->publicData.data, jsonPub->valuestring, jsonObjLen);
                cred->publicData.len = jsonObjLen;
            }

            //Optional Data
            jsonObj = cJSON_GetObjectItem(jsonCred, OIC_JSON_OPTDATA_NAME);
            if (NULL != jsonObj)
            {
                cJSON *jsonOpt = cJSON_GetObjectItem(jsonObj, OIC_JSON_DATA_NAME);
                VERIFY_NOT_NULL(TAG, jsonOpt, ERROR);
                jsonObjLen = strlen(jsonOpt->valuestring);
                cred->optionalData.data =  (uint8_t *)OICCalloc(1, jsonObjLen);
                VERIFY_NOT_NULL(TAG, (cred->optionalData.data), ERROR);
                memcpy(cred->optionalData.data, jsonOpt->valuestring, jsonObjLen);
                cred->optionalData.len = jsonObjLen;

                cJSON *jsonEncoding = cJSON_GetObjectItem(jsonObj, OIC_JSON_ENCODING_NAME);
                VERIFY_NOT_NULL(TAG, jsonEncoding, ERROR);
                cred->optionalData.encoding = GetEncodingTypeFromStr(jsonEncoding->valuestring);

                cJSON *jsonRevstat = cJSON_GetObjectItem(jsonObj, OIC_JSON_REVOCATION_STATUS_NAME);
                VERIFY_NOT_NULL(TAG, jsonRevstat, ERROR);
                cred->optionalData.revstat = jsonObj->valueint;
            }

            //CredUsage
            jsonObj = cJSON_GetObjectItem(jsonCred, OIC_JSON_CREDUSAGE_NAME);
            if (NULL != jsonObj)
            {
                jsonObjLen = strlen(jsonObj->valuestring);
                cred->credUsage = OICStrdup(jsonObj->valuestring);
                VERIFY_NOT_NULL(TAG, (cred->credUsage), ERROR);
            }

#endif // defined(__WITH_DTLS__) || defined(__WITH_TLS__)

            //Period -- Not Mandatory
            jsonObj = cJSON_GetObjectItem(jsonCred, OIC_JSON_PERIOD_NAME);
            if(jsonObj && cJSON_String == jsonObj->type)
            {
                cred->period = OICStrdup(jsonObj->valuestring);
                VERIFY_NOT_NULL(TAG, cred->period, ERROR);
            }
            cred->next = NULL;
        } while( ++idx < numCred);
    }

    // rownerid
    cJSON *jsonCredObj = cJSON_GetObjectItem(jsonCredMap, OIC_JSON_ROWNERID_NAME);
    VERIFY_NOT_NULL(TAG, jsonCredObj, ERROR);
    VERIFY_SUCCESS(TAG, cJSON_String == jsonCredObj->type, ERROR);
    ret = ConvertStrToUuid(jsonCredObj->valuestring, &headCred->rownerID);
    VERIFY_SUCCESS(TAG, OC_STACK_OK == ret, ERROR);
    ret = OC_STACK_OK;

exit:
    cJSON_Delete(jsonRoot);
    if (OC_STACK_OK != ret)
    {
        DeleteCredList(headCred);
        headCred = NULL;
    }
    return headCred;
}

static OicSecAmacl_t* JSONToAmaclBin(const char * jsonStr)
{
    OCStackResult ret = OC_STACK_ERROR;
    OicSecAmacl_t * headAmacl = (OicSecAmacl_t*)OICCalloc(1, sizeof(OicSecAmacl_t));

    cJSON *jsonRoot = NULL;
    cJSON *jsonAmacl = NULL;

    VERIFY_NOT_NULL(TAG, jsonStr, ERROR);

    jsonRoot = cJSON_Parse(jsonStr);
    VERIFY_NOT_NULL(TAG, jsonRoot, ERROR);

    jsonAmacl = cJSON_GetObjectItem(jsonRoot, OIC_JSON_AMACL_NAME);
    VERIFY_NOT_NULL(TAG, jsonAmacl, INFO);

    cJSON *jsonObj = NULL;

    // Resources -- Mandatory
    jsonObj = cJSON_GetObjectItem(jsonAmacl, OIC_JSON_RESOURCES_NAME);
    VERIFY_NOT_NULL(TAG, jsonObj, ERROR);

    // Rlist
    cJSON *jsonRlistArray = cJSON_GetObjectItem(jsonObj, OIC_JSON_RLIST_NAME);
    VERIFY_NOT_NULL(TAG, jsonRlistArray, ERROR);
    VERIFY_SUCCESS(TAG, cJSON_Array == jsonRlistArray->type, ERROR);

    headAmacl->resourcesLen = cJSON_GetArraySize(jsonRlistArray);
    headAmacl->resources = (char**)OICCalloc(headAmacl->resourcesLen, sizeof(char*));
    size_t idxx = 0;
    do
    {
// Needs to be removed once IOT-1746 is resolved.
#ifdef _MSC_VER
#pragma warning(suppress : 4267)
        cJSON *jsonRsrc = cJSON_GetArrayItem(jsonRlistArray, idxx);

#else
        cJSON *jsonRsrc = cJSON_GetArrayItem(jsonRlistArray, idxx);

#endif
        VERIFY_NOT_NULL(TAG, jsonRsrc, ERROR);

        cJSON *jsonRsrcObj = cJSON_GetObjectItem(jsonRsrc, OIC_JSON_HREF_NAME);
        VERIFY_NOT_NULL(TAG, jsonRsrcObj, ERROR);
        VERIFY_SUCCESS(TAG, cJSON_String == jsonRsrcObj->type, ERROR);

        headAmacl->resources[idxx] = OICStrdup(jsonRsrcObj->valuestring);
        VERIFY_NOT_NULL(TAG, (headAmacl->resources[idxx]), ERROR);

    } while ( ++idxx < headAmacl->resourcesLen);

    // Rowner -- Mandatory
    jsonObj = cJSON_GetObjectItem(jsonAmacl, OIC_JSON_ROWNERID_NAME);
    VERIFY_NOT_NULL(TAG, jsonObj, ERROR);
    VERIFY_SUCCESS(TAG, cJSON_String == jsonObj->type, ERROR);

    ret = OC_STACK_OK;

exit:
    cJSON_Delete(jsonRoot);
    if (OC_STACK_OK != ret)
    {
        DeleteAmaclList(headAmacl);
        headAmacl = NULL;
    }
    return headAmacl;
}

OCDeviceProperties* JSONToDPBin(const char *jsonStr)
{
    OIC_LOG(DEBUG, TAG, "JSONToDPBin IN");
    if (NULL == jsonStr)
    {
        return NULL;
    }

    OCStackResult ret = OC_STACK_ERROR;
    OCDeviceProperties *deviceProps = NULL;
    cJSON *jsonDeviceProps = NULL;
    cJSON *jsonObj = NULL;

    cJSON *jsonRoot = cJSON_Parse(jsonStr);
    VERIFY_NOT_NULL(TAG, jsonRoot, ERROR);

    jsonDeviceProps = cJSON_GetObjectItem(jsonRoot, OC_JSON_DEVICE_PROPS_NAME);
    VERIFY_NOT_NULL(TAG, jsonDeviceProps, ERROR);

    deviceProps = (OCDeviceProperties*)OICCalloc(1, sizeof(OCDeviceProperties));
    VERIFY_NOT_NULL(TAG, deviceProps, ERROR);

    // Protocol Independent ID -- Mandatory
    jsonObj = cJSON_GetObjectItem(jsonDeviceProps, OC_RSRVD_PROTOCOL_INDEPENDENT_ID);
    if (jsonObj && (cJSON_String == jsonObj->type))
    {
        OICStrcpy(deviceProps->protocolIndependentId, UUID_STRING_SIZE, jsonObj->valuestring);
    }

    ret = OC_STACK_OK;

exit:
    cJSON_Delete(jsonRoot);
    if (OC_STACK_OK != ret)
    {
        CleanUpDeviceProperties(&deviceProps);
    }
    OIC_LOG_V(DEBUG, TAG, "OUT %s: %s\n", __func__, (deviceProps != NULL) ? "success" : "failure");

    return deviceProps;
}

int main(int argc, char* argv[])
{
    if (argc == 3)
    {
        printf("JSON File Name: %s\n CBOR File Name: %s \n", argv[1], argv[2]);
        ConvertJsonToCBOR(argv[1], argv[2]);
    }
    else
    {
        printf("This program requires two inputs:\n");
        printf("1. First input is a json file tha will be converted to cbor. \n");
        printf("2. Second input is a resulting cbor file that will store converted cbor. \n");
        printf("\t json2cbor <json_file_name> <cbor_file_name>. \n");
    }
}
