
/**
 *
 * Copyright 2005 LogicBlaze Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "stomp.h"

#include "cJSON.h"


#define      USE_TD_WESSEX
//#define USE_TM_SOUTHWEST


#define     MAX_DOWNTIME 600

static int intCrossingCount = 0;

#if 0
int die(int exitCode, const char *message, apr_status_t reason)
{
    char msgbuf[80];
    apr_strerror(reason, msgbuf, sizeof(msgbuf));
    fprintf(stderr, "%s: %s (%d)\n", message, msgbuf, reason);
    exit(exitCode);
    return reason;
}
#endif
#if 0
static void terminate()
{
    apr_terminate();
}
#endif

#if 0
static const char* decode_type(const char *pcaType)
{
    const char * pcaReturn = "Unknown";
    if (strcmp(pcaType, "CA_MSG")==0)
        pcaReturn = "Berth Step";
    else if (strcmp(pcaType, "CB_MSG")==0)
        pcaReturn = "Berth Cancel";
    else if (strcmp(pcaType, "CC_MSG")==0)
        pcaReturn = "Berth Interpose";
    else if (strcmp(pcaType, "CT_MSG")==0)
        pcaReturn = "Heartbeat";
    else if (strcmp(pcaType, "SF_MSG")==0)
        pcaReturn = "Signalling Update";
    else if (strcmp(pcaType, "SG_MSG")==0)
        pcaReturn = "Signalling Refresh";
    else if (strcmp(pcaType, "SH_MSG")==0)
        pcaReturn = "Signalling Refresh Finished";


    return(pcaReturn);
}

static const char* decode_area(const char *pcaArea)
{
    const char * pcaReturn = pcaArea;
    struct area_entry
    {
        const char*pcaArea;
        const char*pcaName;
    };

    struct area_entry areaData[] =
    {
            {"BE", "Basingstoke"},
            {"BP", "Basingstoke (Poole - Wool)"},
            {"BS", "Barnes"},
            {"BX", "Basingstoke (WoE)"},
            {"EH", "Eastleigh"},
            {"FH", "Feltham"},
            {"GD", "Guildford"},
            {"HT", "Havant"},
            {"SU", "Surbiton (Woking)"},
            {"W2", "Woking"},
            {"WI", "Wimbledon NCP"},
            {"ZB", "Bournemouth TDC"},
            {"ZG", "Guildford TDC"},
            {"ZY", "Yeovil TDC"},
    };

#define SIZEOF_AREA (sizeof(areaData)/sizeof(struct area_entry))

    int x;

    for (x=0;x<SIZEOF_AREA;x++)
    {
        if (strcmp(areaData[x].pcaArea, pcaArea)==0)
            pcaReturn = areaData[x].pcaName;
    }


    return(pcaReturn);
}
#endif


struct bit_state_entry
{
    char *pcaAddress;
    int intBit;
    int intState;
    time_t timeOfLastChange;
    struct bit_state_entry *nextState;
};


struct bit_state_entry *headState = NULL;


struct bit_state_entry* getBitState(const char *pcaAddress, int intBit)
{
    struct bit_state_entry *bitState = NULL;

    struct bit_state_entry *currentState = headState;

    while(currentState!=NULL)
    {
        if ((strcmp(currentState->pcaAddress, pcaAddress)==0)&&(currentState->intBit == intBit))
        {
            bitState = currentState;
            break;
        }

        currentState = currentState->nextState;
    }

    if (bitState == NULL)
    {
        bitState = (struct bit_state_entry*)malloc(sizeof(struct bit_state_entry));
        bitState->pcaAddress = strdup(pcaAddress);
        bitState->intBit = intBit;
        bitState->intState = -1; // initial state
        time(&bitState->timeOfLastChange);

        bitState->nextState = headState;
        headState=bitState;
    }

    return(bitState);
}



static const char* decode_data(const char *pcaAddress, const char *pcaTxt)
{
    static char cayReturn[60];

    // get the current bit state

    struct bit_state_entry* bitState;

    int x;

    int val =(int)strtol(pcaTxt, NULL, 16);

    memset(cayReturn,0,sizeof(cayReturn));

    int total = (strlen(pcaTxt)-1)*8;

    for(x=0;x<total; x++)
    {

        int cur = (val >> (total-1-x)) & 0x1;

        bitState = getBitState(pcaAddress, x);

        if (bitState->intState != cur)
        {
            char cayTmp[20];
            time_t curTime;

            time(&curTime);

            // cal the time diff
            time_t diff = curTime -bitState->timeOfLastChange;

#if 0
#define IGNORE_CHANGE_TIMEOUT 5

            // only report the bit if it has changed in the last 3 minutes
            if (diff>(IGNORE_CHANGE_TIMEOUT*60))
            {
                // ignore this change
            }
            // only report if this is not the first change
            else
#endif
            if (bitState->intState>=0)
            {

                // changed
                sprintf(cayTmp, "%d:%s(%d) ", x, cur ? "1" : "0", (int)diff);
                strcat(cayReturn, cayTmp);

            }

            bitState->timeOfLastChange = curTime;
            bitState->intState = cur;
        }
    }
    return(cayReturn);
}


#ifdef USE_TM_SOUTHWEST

#if 0
Wool MOD Siding     596001  WOOLMOD WOOL SDGS   86972
Wool MOD Siding Freightliner Heavy Haul     596002  WOOLFHH WOOLMODSD   86970
Wool MOD Siding GB Railfreight      596003  WOOLGBR     86984
Wool MOD Siding Repair Point                WOOLMODRP   86968
Wool Signal PW5255      596005  WOOL255     86947
Wool Signal PW5264      596006  WOOL264     86948
Wool Signal PW5659      596007  WOOL659     86949
Wool Signal S1      596004  WOOLS1  WOOL SIG1   86986
Wool Signal S11     596003  WOOLS11 WOOLSIG11   86985
#endif

int isInWatchList(const char*pcaLoc)
{
    int x;
    int intRet=0;
    const char *cayWatchList[] =
    {
            "86974", // Wool Winfrith Sidings (WFG)
            "86971", // Wool (WOO)
            "86972", // Wool MOD Siding
            "86970", // Wool MOD Siding Freightliner Heavy Haul
            "86984", // Wool MOD Siding GB Railfreight
            "86968", // Wool MOD Siding Repair Point
            "86947", // Wool Signal PW5255
            "86948", // Wool Signal PW5264
            "86949", // Wool Signal PW5659
            "86986", // Wool Signal S1
            "86985", // Wool Signal S11
    };

#define SIZEOF_WATCHLIST (sizeof(cayWatchList)/sizeof(const char *))

    for (x=0; x<SIZEOF_WATCHLIST;x++)
    {
        if (strcmp(cayWatchList[x], pcaLoc) == 0)
            intRet = 1;
    }

    return(intRet);
}


const char* getHumanLoc(const char*pcaLoc)
{
    const char *pcaReturn = "Unknown";
    int x;
    int intRet=0;

    struct loc_entry
    {
        const char*pcaLoc;
        const char*pcaName;
    };

    struct loc_entry locList[] =
    {
            {"86974", "Wool Winfrith Sidings (WFG)"},
            {"86971", "Wool (WOO)"},
            {"86972", "Wool MOD Siding"},
            {"86970", "Wool MOD Siding Freightliner Heavy Haul"},
            {"86984", "Wool MOD Siding GB Railfreight"},
            {"86968", "Wool MOD Siding Repair Point"},
            {"86947", "Wool Signal PW5255"},
            {"86948", "Wool Signal PW5264"},
            {"86949", "Wool Signal PW5659"},
            {"86986", "Wool Signal S1"},
            {"86985", "Wool Signal S11"},
    };

#define SIZEOF_WATCHLIST (sizeof(locList)/sizeof(struct loc_entry ))

    for (x=0; x<SIZEOF_WATCHLIST;x++)
    {
        if (strcmp(locList[x].pcaLoc, pcaLoc) == 0)
        {
            pcaReturn = locList[x].pcaName;
        }
    }

    return(pcaReturn);
}

static void parse_responce(const char *pcaResponce)
{
    cJSON *json;

    json = cJSON_Parse(pcaResponce);

    if (!json)
    {
        printf("Error before: [%s]\n", cJSON_GetErrorPtr());
    }
    else
    {
        int intTopSize = cJSON_GetArraySize(json);
        int x;

        for (x = 0; x < intTopSize; x++)
        {
            cJSON *arrayEntry = cJSON_GetArrayItem(json, x);

            if (arrayEntry != NULL)
            {
                cJSON *headerObject = cJSON_GetObjectItem(arrayEntry, "header");

                if (headerObject != NULL)
                {
                    //objectItem = objectItem->child;

                    //if (objectItem->type == cJSON_Object)
                    {
                        int intMsgType = -1;

                        cJSON *msgType = cJSON_GetObjectItem(headerObject, "msg_type");

                        if (msgType)
                            intMsgType = atoi(msgType->valuestring);

                        // check for 0003 - Train Movement
                        if (intMsgType == 3)
                        {
                            // ok, now get the body
                            cJSON *bodyObject = cJSON_GetObjectItem(arrayEntry, "body");
                            if (bodyObject!=NULL)
                            {
                                const char *pcaEventType = "Unknown";
                                const char *pcaTrainID = "Unknown";
                                const char *pcaSTANOXLoc = "Unknown";
                                const char *pcaTimeStamp = "0";


                                cJSON *event_type = cJSON_GetObjectItem(bodyObject, "event_type");
                                if (event_type!=NULL)
                                    pcaEventType = event_type->valuestring;

                                cJSON *train_id = cJSON_GetObjectItem(bodyObject, "train_id");
                                if (train_id!=NULL)
                                    pcaTrainID = train_id->valuestring;


                                cJSON *loc_stanox = cJSON_GetObjectItem(bodyObject, "loc_stanox");
                                if (loc_stanox!=NULL)
                                    pcaSTANOXLoc = loc_stanox->valuestring;

                                cJSON *actual_timestamp = cJSON_GetObjectItem(bodyObject, "actual_timestamp");
                                if (actual_timestamp!=NULL)
                                {
                                    pcaTimeStamp = actual_timestamp->valuestring;
                                }



                                // ok, check for any locations in the watch list
                               // if (isInWatchList(pcaSTANOXLoc))
                                {
                                    char cayTimebuf[30];
                                    time_t rawtime;
                                    struct tm * timeinfo;

                                    unsigned long long lngVal = strtoull(pcaTimeStamp, NULL, 10);

                                    time_t intTime = (time_t)((unsigned long long)lngVal/1000);
                                    timeinfo = localtime ( &intTime );

                                    asctime_r (timeinfo, cayTimebuf);
                                    cayTimebuf[19]=0;

                                    printf("%s: Train %s, Event Type: %s, Loc %s, (%s)\r\n", cayTimebuf, pcaTrainID, pcaEventType, pcaSTANOXLoc, getHumanLoc(pcaSTANOXLoc));
                                }

                            }


                        }
                        else
                        {
                            printf("Message Type %d\n", intMsgType);
                        }
                    }
                }
            }
        }

        cJSON_Delete(json);
    }

}


#else

enum crossing_state
{
    state_up,
    state_down,
    state_closing,
    state_opening,
};

#define SIMPLE_MODE

struct train_entry
{
    char cayName[200];
    char pcaLoc[200];
    time_t seenTime;


    int intLastDownTime;
    unsigned long long ulgRunningDownTime;
    int intNumDown;
    int intAvgDown;
    time_t downDown;
    time_t upTime;

    struct train_entry *nextTrain;
};

struct train_entry *trainHead = NULL;

// get an entry
struct train_entry *getTrain(const char*pcaName)
{
    struct train_entry *retTrain =NULL;
    struct train_entry *curTrain = trainHead;

    while(curTrain!=NULL)
    {
        if (strcmp(curTrain->cayName,pcaName)==0)
        {
            retTrain = curTrain;
            break;
        }
        curTrain=curTrain->nextTrain;
    }

    if (retTrain==NULL)
    {
        retTrain =(struct train_entry*)malloc(sizeof(struct train_entry));
        memset(retTrain, 0, sizeof(struct train_entry));
        strcpy(retTrain->cayName,pcaName);
        strcpy(retTrain->pcaLoc, "");

        retTrain->nextTrain = trainHead;
        trainHead = retTrain;
    }

    return(retTrain);
}

// the number of seconds before we consider a train
// no longer in the given location
#define SEEN_TIMEOUT        (10*60)

int crossingState(const char *pcaID, const char *pcaTo, int *intReturnMaxDown)
{
    int intRes=0;
    int x;

    int intOldCount = intCrossingCount;

    time_t currentTime;

    // see:
    // http://www.opentraintimes.com/maps/signalling/swml7

#ifdef SIMPLE_MODE

    // a simple mode that if a train is in one of these, then
    // it is considered "active"
    const char *pcaActiveList[] =
    {
            // Poole to Waymouth
            "5253",
            "5261",
            // Weymouth to poole.
            "5264",
            "5268",
    };
#define SIZEOF_ACTIVE_LIST (sizeof(pcaActiveList)/sizeof( const char * ))

    // get the train
    // and update its data
    struct train_entry *selectedTrain = getTrain(pcaID);
    strcpy(selectedTrain->pcaLoc,pcaTo);


    currentTime = time(NULL);

    selectedTrain->seenTime = currentTime;

    intCrossingCount=0;

    // ok, now loop around all trains and see how many are in "active" locations
    struct train_entry *curTrain = trainHead;

    while(curTrain!=NULL)
    {
        time_t diffTime = currentTime - curTrain->seenTime;

        // Ignore trains that have not seen a
        // change for SEEN_TIMEOUT seconds
        if (diffTime < SEEN_TIMEOUT )
        {
            int intFoundInActiveList = 0;

            // first scan the active list, this looks in the active
            // list and sees if it is there
            for (x=0; x<SIZEOF_ACTIVE_LIST;x++)
            {
                if (strcmp(pcaActiveList[x], curTrain->pcaLoc) == 0)
                {
                    // it was found in the active list, so
                    // make sure we say it is still avtive
                    intFoundInActiveList=1;
                }

            }


            if (intFoundInActiveList)
            {

                // ok, we have a new activation,
                // so we must increment the counter
                intCrossingCount++;

                // is this the selected train
                if (selectedTrain == curTrain)
                {
                    // detected train in the list, so it is currently
                    // active, we need to start the train "down" timer
                    if (curTrain->downDown==0)
                            curTrain->downDown=time(NULL);

                }

                time_t timeGone = (time_t)(currentTime - curTrain->downDown);

                printf("train %s is in active list, avgDown=%lld, sinceDown=%lld \n",
                        curTrain->cayName, (long long)curTrain->intAvgDown,
                        (long long)timeGone);

                // ok, calculate the time this train will exist the "watch" area

                // if we have a valid avg for this train?
                if (curTrain->intAvgDown!=0)
                {
                    // yes , we have an average traval time over the watch area,
                    // so we need to calculate how far into the avg we are
                    // cal the time since down

                    // and now cal left time
                    int timeLeft = curTrain->intAvgDown - timeGone;

                    // and see if we have a new MAX waiting time.
                    if (timeLeft > *intReturnMaxDown)
                        *intReturnMaxDown = timeLeft;
                }
            }
            else
            {
                // ok, the current train is not in the "watch" location
                // is this the selected train
                // if it is then we need to see if it has just moved out
                // of the watch location
                if (selectedTrain == curTrain)
                {
                    // was it down and now up?
                    if (curTrain->downDown!=0)
                    {
                        curTrain->upTime=time(NULL);

                        // call the down time
                        curTrain->intLastDownTime = curTrain->upTime - curTrain->downDown;

                        if (curTrain->intLastDownTime< MAX_DOWNTIME)
                        {

                            // add to avg
                            curTrain->ulgRunningDownTime+=curTrain->intLastDownTime;
                            curTrain->intNumDown++;

                            curTrain->intAvgDown = curTrain->ulgRunningDownTime/curTrain->intNumDown;


                            printf("train %s has moved out of the active list, avgDown=%lld, lastDownTime=%lld, numDown=%d\n",
                                                    curTrain->cayName, (long long)curTrain->intAvgDown,
                                                    (long long)curTrain->intLastDownTime,
                                                    curTrain->intNumDown);
                        }
                        else
                        {
                            printf("train %s has moved out of the active list, but is was in the watch zone for too long\n", curTrain->cayName);
                        }

                        // clear out down
                        curTrain->downDown=0;
                        curTrain->upTime=0;
                    }
                }
            }
        }
        curTrain=curTrain->nextTrain;
    }

#else

    const char *pcaActiveList[] =
    {
            "5253",
            // observed: Change the 5264 to 5268 .
            // Weymouth to poole.
            //"5264",
            "5268",
    };

#define SIZEOF_ACTIVE_LIST (sizeof(pcaActiveList)/sizeof( const char * ))

    const char *pcaInActiveList[] =
    {
            "5267",
            //"5256", // a bit early
            "5252",
    };

#define SIZEOF_INACTIVE_LIST (sizeof(pcaInActiveList)/sizeof( const char * ))

    // first scan the active list
    for (x=0; x<SIZEOF_ACTIVE_LIST;x++)
    {
        if (strcmp(pcaActiveList[x], pcaTo) == 0)
        {
            // ok, we have a new activation,
            // so we must increment the counter
            intCrossingCount++;
            if (intCrossingCount>2)
                intCrossingCount=2;
        }
    }

    // now scan the inactive list

    for (x=0; x<SIZEOF_INACTIVE_LIST;x++)
    {
        if (strcmp(pcaInActiveList[x], pcaTo) == 0)
        {
            // we have one less activation, so decrement the count
            intCrossingCount--;
            if (intCrossingCount<0)
                intCrossingCount=0;
        }
    }
#endif

    // ok, was 0 and now zero
    // - up and remains up
    if ((intOldCount==0) && (intCrossingCount==0))
           intRes=state_up;
    // ok, was 0, now non-zero
    // - closing
    else if ((intOldCount==0) && (intCrossingCount>0))
        intRes=state_closing;
    // ok, was 1, now 1
    else if ((intOldCount>0) && (intCrossingCount>0))
        intRes=state_down;
    // ok, was 1 and now 0
    else if ((intOldCount>0) && (intCrossingCount==0))
        intRes=state_opening;

    return(intRes);
}

const char* crossingStateString(int intNewState)
{
    const char*  pcaReturn = "Unknown";
    switch(intNewState)
    {
        case state_up:
            pcaReturn="Up";
            break;
        case state_down:
            pcaReturn="Down";
            break;
        case state_closing:
            pcaReturn="Closing";
            break;
        case state_opening:
            pcaReturn="Opening";
            break;
    }
    return(pcaReturn);
}


void set_state(int intNewState, int intOpeningIn)
{
    FILE *stateFile = fopen("/var/log/train-track.state", "w");

    if (stateFile!=NULL)
    {
        fprintf(stateFile, "{\"state\": %d, \"opentime\": %d}\n", intNewState, intOpeningIn);

        fclose(stateFile);
    }
}


static void parse_responce(const char *pcaResponce)
{
    cJSON *json;

    json = cJSON_Parse(pcaResponce);

    if (!json)
    {
        printf("Error before: [%s]\n", cJSON_GetErrorPtr());
    }
    else
    {
        //char *out;
        //out=cJSON_Print(json);

        //printf("%s\n",out);
        //free(out);

        // ok, its an SF message
        // Signalling Update    Updates the signalling data for a specified set of signalling elements

        //  printf("this %s %d\r\n", json->string, json->type);
        ///  printf("child %s %d\r\n", json->child->string, json->child->type);
        // printf("child child %s %d\r\n", json->child->child->string, json->child->child->type);

        int intTopSize = cJSON_GetArraySize(json);
        int x;

        for (x = 0; x < intTopSize; x++)
        {
            cJSON *arrayEntry = cJSON_GetArrayItem(json, x);

            if (arrayEntry != NULL)
            {
                if (arrayEntry->type == cJSON_Object)
                {


                    // ok, its an SF message
                    // Signalling Update    Updates the signalling data for a specified set of signalling elements

                   //printf("name %s\r\n", arrayEntry->child->string);

                   // if (arrayEntry->child->string[0] == 'S')
                    {
                        char *pcaMessageType = (arrayEntry->child->string);
                        //printf("Message type %s\r\n", arrayEntry->child->string);

                        cJSON *objectItem = arrayEntry->child;  //cJSON_GetObjectItem(arrayEntry, "SF_MSG");

                        // get the type, this is the first field in the list
                        if (objectItem != NULL)
                        {
                            char cayTimebuf[30];
                            const char *pcaAreaID = NULL;
                            const char *pcaTime = NULL;
                            time_t intTime = 0;

                            cayTimebuf[0] =0;

                            // ok, this is the one we are looking for
                            // so now find the other data
                            cJSON *areaID = cJSON_GetObjectItem(objectItem, "area_id");

                            if (areaID != NULL)
                            {
                                pcaAreaID = (areaID->valuestring);
                            }

                            cJSON *jsonTime = cJSON_GetObjectItem(objectItem, "time");

                            if (jsonTime != NULL)
                            {
                                pcaTime = (jsonTime->valuestring);

                                struct tm * timeinfo;

                                unsigned long long lngVal = strtoull(pcaTime, NULL, 10);

                                intTime = (time_t)((unsigned long long)lngVal/1000);
                                timeinfo = localtime ( &intTime );

                                asctime_r (timeinfo, cayTimebuf);
                                cayTimebuf[19]=0;
                            }

                            // ok, filter on the one we want
                            if (strcmp(pcaAreaID, "BP")==0)
                            {

                                // check for class S
                                if (pcaMessageType[0] == 'S')
                                {
                                    const char *pcaAddress = NULL;
                                    const char *pcaData = NULL;
#if 0
                                    char *out=cJSON_Print(objectItem);
                                    printf("%s\n",out);
                                    free(out);
#endif

                                    // get the address
                                    cJSON *address = cJSON_GetObjectItem(objectItem, "address");

                                    if (address != NULL)
                                    {
                                        // Address of the signalling data being updated. For an SF_MSG, this is
                                        // the individual signalling element. For an SG_MSG or SH_MSG, this is the
                                        // starting address for the four bytes supplied in the data field.
                                        pcaAddress = (address->valuestring);
                                    }

                                    cJSON *data = cJSON_GetObjectItem(objectItem, "data");

                                    if (data != NULL)
                                    {
                                        // Signalling data
                                        pcaData = (data->valuestring);
                                    }


                                    // only show updates
                                    // SF=Signalling Update
                                    if ((strcmp(pcaMessageType, "SF_MSG")==0))
                                    {
                                        const char *pcaDataMe = decode_data(pcaAddress, pcaData);

                                        if (strlen(pcaDataMe)>0)
                                        {

                                            // ok, we have pool +wool?
                                            printf("%s: Address %s, Data %s\r\n",
                                                            cayTimebuf,
                                                          //decode_area(pcaAreaID), decode_type(pcaMessageType),
                                                          pcaAddress, pcaDataMe);

                                        }
                                    }
                                    else
                                    {
                                        printf("%s: Unknown msg type %s\r\n",
                                                cayTimebuf,
                                                pcaMessageType);
                                    }


                                    // see http://nrodwiki.rockshore.net/index.php/Decoding_S-Class_Data
                                    // for how to decode

                                }
                                // must be class C
                                else if (pcaMessageType[0] == 'C')
                                {
                                    const char *pcaFrom = NULL;
                                    const char *pcaTo = NULL;
                                    const char *pcaDescr = NULL;

                                    cJSON *from = cJSON_GetObjectItem(objectItem, "from");
                                    if (from != NULL)
                                        pcaFrom = from->valuestring;

                                    cJSON *to = cJSON_GetObjectItem(objectItem, "to");
                                    if (to != NULL)
                                        pcaTo = to->valuestring;

                                    cJSON *descr = cJSON_GetObjectItem(objectItem, "descr");
                                    if (descr != NULL)
                                        pcaDescr = descr->valuestring;

                                    // The CA message is a 'step' message.
                                    if ((strcmp(pcaMessageType, "CA_MSG")==0))
                                    {
                                        char pcaAvgTime[100];
                                        pcaAvgTime[0]=0;
                                        // the TD, From berth (see TD berths page)
                                        // is a code, it needs to be converted to a valid location
                                        // currently dont know where to get the lookup data

                                        int intMaxDown = -1;

                                        int intNewState = crossingState(pcaDescr, pcaTo, &intMaxDown);

                                        // ok, we need to check the state
                                        if (intMaxDown>=0)
                                        {
                                            sprintf(pcaAvgTime,", Opening in %d", intMaxDown);
                                        }

                                        printf("%s: Train %s, From %s, To %s - %s %s\r\n",
                                                cayTimebuf,
                                                pcaDescr,
                                                pcaFrom, pcaTo,
                                                crossingStateString(intNewState),
                                                pcaAvgTime);

                                        set_state(intNewState, intMaxDown);
                                    }

                                }
                            }
                        }
                    }
                }
            }
        }

        cJSON_Delete(json);

    }

}
#endif


static apr_pool_t *pool = NULL;
static stomp_connection *connection=NULL;

int create_link()
{
    int intRes = 0;
    apr_status_t rc;


    setbuf(stdout, NULL);

    rc = apr_initialize();
    if (rc != APR_SUCCESS)
    {
        printf("Could not initilize\n");
    }
    else
    {

        rc = apr_pool_create(&pool, NULL);

        if (rc != APR_SUCCESS)
        {
            printf("Could not allocate pool\n");
        }
        else
        {

            fprintf(stdout, "Connecting......");
            rc = stomp_connect(&connection, "datafeeds.networkrail.co.uk", 61618, pool);

            if (rc != APR_SUCCESS )
            {
                printf("Could not connect\n");
            }
            else
            {


                // ok, we want a timeout
                rc = apr_socket_timeout_set( connection->socket, 120*APR_USEC_PER_SEC);

                if (rc != APR_SUCCESS )
                {
                    printf("Could not set timeout for connection\n");
                }
                else
                {

                    fprintf(stdout, "Sending connect message.");
                    {
                        stomp_frame frame;
                        frame.command = "CONNECT";
                        frame.headers = apr_hash_make(pool);
                        apr_hash_set(frame.headers, "login", APR_HASH_KEY_STRING, "XXXXXXXXXXXXXXXXXXXx");
                        apr_hash_set(frame.headers, "passcode", APR_HASH_KEY_STRING, "XXXXXXXXXXXXXXXXX");
                        frame.body = NULL;
                        rc = stomp_write(connection, &frame, pool);
                    }

                    if (rc != APR_SUCCESS )
                    {
                        fprintf(stdout, "Could not send frame\n");
                    }
                    else
                    {
                        fprintf(stdout, "OK\n");
                        fprintf(stdout, "Reading Response.");

                        {
                            stomp_frame *frame;
                            rc = stomp_read(connection, &frame, pool);
                            if (rc == APR_SUCCESS)
                            {
                                fprintf(stdout, "Response: %s, %s\n", frame->command, frame->body);
                            }
                        }

                        if (rc != APR_SUCCESS )
                        {
                            fprintf(stdout, "Could not read frame\n");
                        }
                        else
                        {
                            fprintf(stdout, "OK\n");

                            fprintf(stdout, "Sending Subscribe.");
                            {
                                stomp_frame frame;
                                frame.command = "SUB";
                                frame.headers = apr_hash_make(pool);

#if defined(USE_TD_WESSEX)
                                // the Wessex area
                                apr_hash_set(frame.headers, "destination", APR_HASH_KEY_STRING, "/topic/TD_WESS_SIG_AREA");
#elif defined(USE_TD_WESTERN_WEST_COUNTRY)
                                // Western West Country
                                // Didcot, Swindon
                                apr_hash_set(frame.headers, "destination", APR_HASH_KEY_STRING, "/topic/TD_WWC_SIG_AREA");
#elif defined(USE_TD_WEST_COAST_COUNTRY)
                                // West Coast South
                                // Rugby SCC, Wembley
                                apr_hash_set(frame.headers, "destination", APR_HASH_KEY_STRING, "/topic/TD_WCS_SIG_AREA");
#elif defined(USE_TM_SOUTHWEST)
                                // TRain movements, - South West Trains HY  84  SW
                                apr_hash_set(frame.headers, "destination", APR_HASH_KEY_STRING, "/topic/TRAIN_MVT_HY_TOC");
#else
#error no loc defined
#endif

                                frame.body = NULL;
                                rc = stomp_write(connection, &frame, pool);

                            }

                            if (rc != APR_SUCCESS)
                            {
                                fprintf(stdout, "Could not send frame\n");
                            }
                            else
                            {
                                fprintf(stdout, "OK\n");

                                intRes=1;
                            }
                        }
                    }
                }
            }
        }
    }

    return(intRes);
}

void disconnect_link()
{
    apr_status_t rc;

    if (connection!=NULL)
    {
        fprintf(stdout, "Sending Disconnect.");
        {
            stomp_frame frame;
            frame.command = "DISCONNECT";
            frame.headers = NULL;
            frame.body = NULL;
            rc = stomp_write(connection, &frame, pool);

        }
        if (rc != APR_SUCCESS )
        {
            fprintf(stdout, "Could not send frame\n");
        }

        fprintf(stdout, "Disconnecting...");
        rc = stomp_disconnect(&connection);

        if (rc != APR_SUCCESS )
        {
            fprintf(stdout, "Could not disconnect\n");
        }

        connection=NULL;
    }

    if (pool!=NULL)
        apr_pool_destroy(pool);
    pool=NULL;

    apr_terminate();
}

int main()
{

    while(1)
    {
        if (create_link())
        {
            apr_status_t rc;
            int intExitLoop=0;

            while (intExitLoop == 0)
            {
                {
                    stomp_frame *frame;
                    rc = stomp_read(connection, &frame, pool);
                    if (rc != APR_SUCCESS)
                    {
                        fprintf(stdout, "Read Timeout\n");
                        intExitLoop=1;
                    }
                    else
                    {
                        parse_responce(frame->body);
                    }
                }
            }
        }

        // always make sure we disconnect
        disconnect_link();

        // and sleep to prevent hog
        sleep(100);
    }

    return 0;
}


