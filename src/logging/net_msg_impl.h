#pragma once

#include "logging/logging.h"
#include "network_core/peers.h"


// Request: ranges of log ID
void qLogger::processRequestLog(Peer* peer, RequestResponseHeader* header)
{
#if ENABLED_LOGGING
    RequestLog* request = header->getPayload<RequestLog>();
#if !defined(NDEBUG) && !defined(NO_UEFI)
    CHAR16 dbgMsgBuf[300];
    setText(dbgMsgBuf, L"processRequestLog(): fromID=");
    appendNumber(dbgMsgBuf, request->fromID, FALSE);
    appendText(dbgMsgBuf, L", toID=");
    appendNumber(dbgMsgBuf, request->toID, FALSE);
    addDebugMessage(dbgMsgBuf);
#endif
    if (request->passcode[0] == logReaderPasscodes[0]
        && request->passcode[1] == logReaderPasscodes[1]
        && request->passcode[2] == logReaderPasscodes[2]
        && request->passcode[3] == logReaderPasscodes[3])
    {
        BlobInfo startIdBufferRange = logBuf.getBlobInfo(request->fromID);
        BlobInfo endIdBufferRange = logBuf.getBlobInfo(request->toID); // inclusive
        if (startIdBufferRange.startIndex != -1 && startIdBufferRange.length != -1
            && endIdBufferRange.startIndex != -1 && endIdBufferRange.length != -1)
        {
            unsigned long long toID = request->toID;
            if (endIdBufferRange.startIndex < startIdBufferRange.startIndex)
            {
                // round buffer case, only response first packet - let the client figure out and request the rest
                for (unsigned long long i = request->fromID + 1; i <= request->toID; i++)
                {
                    BlobInfo iBufferRange = logBuf.getBlobInfo(i);
                    ASSERT(iBufferRange.startIndex >= 0);
                    ASSERT(iBufferRange.length >= 0);
                    if (iBufferRange.startIndex < startIdBufferRange.startIndex)
                    {
                        toID = i - 1;
                        endIdBufferRange = logBuf.getBlobInfo(toID);
                        break;
                    }
                }
                // first packet: from startID to end of buffer IS SENT BELOW
                // second packet: from start buffer to endID IS NOT SENT FROM HERE, but requested by client later

#if !defined(NDEBUG) && !defined(NO_UEFI)
                addDebugMessage(L"processRequestLog() round buffer case: only send subset of requested IDs");
                setText(dbgMsgBuf, L"startIdOffset=");
                appendNumber(dbgMsgBuf, startIdBufferRange.startIndex, TRUE);
                appendText(dbgMsgBuf, L", endIdOffset=");
                appendNumber(dbgMsgBuf, endIdBufferRange.startIndex, TRUE);
                appendText(dbgMsgBuf, L", endIdLength=");
                appendNumber(dbgMsgBuf, endIdBufferRange.length, TRUE);
                addDebugMessage(dbgMsgBuf);
#endif
            }

            long long startFrom = startIdBufferRange.startIndex;
            long long length = endIdBufferRange.length + endIdBufferRange.startIndex - startFrom;
            if (length > RequestResponseHeader::max_size)
            {
#if !defined(NDEBUG) && !defined(NO_UEFI)
                addDebugMessage(L"processRequestLog() too long message of ");
                appendNumber(dbgMsgBuf, length, TRUE);
                setText(dbgMsgBuf, L" bytes starting at offset ");
                appendNumber(dbgMsgBuf, startFrom, TRUE);
                addDebugMessage(dbgMsgBuf);
#endif
                length -= endIdBufferRange.length;
                while (length > RequestResponseHeader::max_size)
                {
                    ASSERT(toID > request->fromID);
                    --toID;
                    endIdBufferRange = logBuf.getBlobInfo(toID);
                    length -= endIdBufferRange.length;
                }
            }
#if !defined(NDEBUG) && !defined(NO_UEFI)
            addDebugMessage(L"processRequestLog() sending ");
            appendNumber(dbgMsgBuf, length, TRUE);
            setText(dbgMsgBuf, L" bytes starting at offset ");
            appendNumber(dbgMsgBuf, startFrom, TRUE);
            addDebugMessage(dbgMsgBuf);
#endif
            enqueueResponse(peer, (unsigned int)(length), RespondLog::type, header->dejavu(), logBuffer + startFrom);
        }
        else
        {
            enqueueResponse(peer, 0, RespondLog::type, header->dejavu(), NULL);
        }
        return;
    }
#endif
    enqueueResponse(peer, 0, RespondLog::type, header->dejavu(), NULL);
}

void qLogger::processRequestTxLogInfo(Peer* peer, RequestResponseHeader* header)
{
#if ENABLED_LOGGING
    RequestLogIdRangeFromTx* request = header->getPayload<RequestLogIdRangeFromTx>();
    if (request->passcode[0] == logReaderPasscodes[0]
        && request->passcode[1] == logReaderPasscodes[1]
        && request->passcode[2] == logReaderPasscodes[2]
        && request->passcode[3] == logReaderPasscodes[3]
        && request->tick >= tickBegin
        )
    {
        ResponseLogIdRangeFromTx resp;
        if (request->tick <= lastUpdatedTick)
        {
            if (request->tick < tickLoadedFrom)
            {
                // unknown logging because the whole node memory is loaded from files
                resp.fromLogId = -2;
                resp.length = -2;
            }
            else
            {
                BlobInfo info = tx.getLogIdInfo(request->tick, request->txId);
                resp.fromLogId = info.startIndex;
                resp.length = info.length;
            }
        }
        else
        {
            // logging of this tick hasn't generated
            resp.fromLogId = -3;
            resp.length = -3;
        }
        
        enqueueResponse(peer, sizeof(ResponseLogIdRangeFromTx), ResponseLogIdRangeFromTx::type, header->dejavu(), &resp);
        return;
    }
#endif
    enqueueResponse(peer, 0, ResponseLogIdRangeFromTx::type, header->dejavu(), NULL);
}

void qLogger::processRequestTickTxLogInfo(Peer* peer, RequestResponseHeader* header)
{
#if ENABLED_LOGGING
    RequestAllLogIdRangesFromTick* request = header->getPayload<RequestAllLogIdRangesFromTick>();
    if (request->passcode[0] == logReaderPasscodes[0]
        && request->passcode[1] == logReaderPasscodes[1]
        && request->passcode[2] == logReaderPasscodes[2]
        && request->passcode[3] == logReaderPasscodes[3]
        && request->tick >= tickBegin
        )
    {
        ResponseAllLogIdRangesFromTick resp;
        int txId = 0;
        if (request->tick <= lastUpdatedTick)
        {
            if (request->tick < tickLoadedFrom)
            {
                // unknown logging because the whole node memory is loaded from files
                for (txId = 0; txId < LOG_TX_PER_TICK; txId++)
                {
                    resp.fromLogId[txId] = -2;
                    resp.length[txId] = -2;
                }
            }
            else
            {
                for (txId = 0; txId < LOG_TX_PER_TICK; txId++)
                {
                    BlobInfo info = tx.getLogIdInfo(request->tick, txId);
                    resp.fromLogId[txId] = info.startIndex;
                    resp.length[txId] = info.length;
                }
            }
        }
        else
        {
            // logging of this tick hasn't generated
            for (txId = 0; txId < LOG_TX_PER_TICK; txId++)
            {
                resp.fromLogId[txId] = -3;
                resp.length[txId] = -3;
            }
        }    
        enqueueResponse(peer, sizeof(ResponseAllLogIdRangesFromTick), ResponseAllLogIdRangesFromTick::type, header->dejavu(), &resp);
        return;
    }
#endif
    enqueueResponse(peer, 0, ResponseAllLogIdRangesFromTick::type, header->dejavu(), NULL);
}
