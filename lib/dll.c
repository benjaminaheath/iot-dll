#include "dll.h"

static DLL dll = {DLL_MAC_RECV, NULL, 0, WAIT, NULL, 0, false, false};

void send_dll(NET_packet pkt){
    /* 
        Take contents of net pkt to build frame
        Call send_phy to transmit over physical layer
    */
    // prepare phy for transmission and receive 
    recv_callback_phy recv = phy_phy();

    // packet fragmentation
    uint8_t num_frames = __get_num_pkt_fragments(pkt.pkt_size,DLL_PAYLOAD_MAX);
    uint8_t* len_payloads = __get_len_pkt_fragments(pkt.pkt_size,DLL_PAYLOAD_MAX);
    uint8_t** fragments = __get_pkt_fragments(len_payloads,num_frames,pkt.packet,pkt.pkt_size);

    for(int f = 0; f < num_frames; ++f){
        // Specify all framing information, then pass to framing helper
        DLL_frame frm;
        frm.frame = NULL;
        frm.frame_len = 0;

        // CTRL Information
        frm.FRAGMENT  = f;
        frm.FINAL     = (f == num_frames-1);
        frm.PROTOCOL  = DLL_PROTOCOL;
        frm.MSG_TYPE  = MSG;
        frm.CHECKTYPE = DLL_CHECKSUM;
        frm.SEQ_NUM   = 0;

        // ADDR Information
        frm.ADDR_SEND = DLL_MAC_SEND;
        frm.ADDR_RECV = DLL_MAC_RECV;

        // Length Information
        frm.LENGTH = len_payloads[f];

        // Payload Information
        frm.PAYLOAD = fragments[f];

        // Hand to Framing Helper to complete rest of framing
        __frame_dll(&frm);

        // hand frame to PHY for transmission
        for(size_t f = 0; f < frm.frame_len; ++f){send_phy(frm.frame[f], recv);}

        // clean up and free resources after transmission
        free(frm.frame);
    }
}

recv_callback_dll link_dll(){
    return recv_dll;
}

//TODO: Implement function calls in each mode to receive frame and reset
void recv_dll(uint8_t byte){
    enum buf_mode next_mode;

    switch (dll.mode)
    {
    case WAIT:
        if(byte == 0x7E){next_mode = LISTEN;}
        else{next_mode = WAIT;}
        break;
    case LISTEN: 
        if(byte == 0x7D){next_mode = ESCAPE;}
        else if(byte == 0x7E){__recv_frame(); next_mode = WAIT;}
        else{append_byte(&dll.buf,&dll.buf_size,byte); next_mode = LISTEN;}
        break;
    case ESCAPE:
        next_mode = LISTEN;
        break;
    }
    dll.mode = next_mode;
}

// Helper to frame DLL after all frame information set
void __frame_dll(DLL_frame* frm){
    // Generate CTRL subframe
    uint16_t CTRL      = __get_control_subframe(frm);
    uint8_t  CTRL_HIGH = (CTRL) >> 8;
    uint8_t  CTRL_LOW  = (CTRL)     ;

    // Build pre-Escaped, pre-Delimited Frame
    append_byte(&frm->frame,&frm->frame_len,CTRL_HIGH);
    append_byte(&frm->frame,&frm->frame_len,CTRL_LOW);
    append_byte(&frm->frame,&frm->frame_len,frm->ADDR_SEND);
    append_byte(&frm->frame,&frm->frame_len,frm->ADDR_RECV);
    append_byte(&frm->frame,&frm->frame_len,frm->LENGTH);
    append_bytes(&frm->frame,&frm->frame_len,frm->PAYLOAD,frm->LENGTH);

    // Append checksum of pre-Escaped, pre-Delimited Frame
    uint16_t CHECKSUM     = __get_checksum_subframe(frm->frame,frm->frame_len);
    uint8_t  CHECKSUM_HIGH = (CHECKSUM) >> 8;
    uint8_t  CHECKSUM_LOW  = (CHECKSUM)     ;

    append_byte(&frm->frame,&frm->frame_len,CHECKSUM_HIGH);
    append_byte(&frm->frame,&frm->frame_len,CHECKSUM_LOW);


    // Build Escaped, pre-Delimited Frame
    __escape_frame(&frm->frame,&frm->frame_len);

    // Build Escaped, Delimited Frame
    insert_byte(&frm->frame,&frm->frame_len,DLL_HEAD_BYTE,0);
    append_byte(&frm->frame,&frm->frame_len,DLL_FOOT_BYTE);   // Append Footer byte at end

}

// Helper to deframe DLL from byte stream and extract frame information
void __deframe_dll(DLL_frame* frm){
    // Get CTRL Information
    uint8_t CTRL_HIGH = get_byte(frm->frame,frm->frame_len,0);
    uint8_t CTRL_LOW  = get_byte(frm->frame,frm->frame_len,1);

    frm->FRAGMENT  = (0xE0 & CTRL_HIGH) >> 5; // BBB00000
    frm->FINAL     = (0x10 & CTRL_HIGH) >> 4; // 000B0000
    frm->PROTOCOL  = (0x08 & CTRL_HIGH) >> 3; // 0000B000
    frm->MSG_TYPE  = (0x06 & CTRL_HIGH) >> 1; // 00000BB0
    frm->CHECKTYPE = (0x01 & CTRL_HIGH)     ; // 0000000B
    frm->SEQ_NUM   = (0xFF & CTRL_LOW )     ; // BBBBBBBB

    // Get ADDR information
    frm->ADDR_RECV = get_byte(frm->frame,frm->frame_len,2);
    frm->ADDR_SEND = get_byte(frm->frame,frm->frame_len,3);

    // Get LENGTH information
    frm->LENGTH  = get_byte(frm->frame,frm->frame_len,4);

    // Get PAYLOAD
    frm->PAYLOAD = get_bytes_from(frm->frame,frm->frame_len,5,frm->LENGTH);

    uint8_t CHECKSUM_HIGH = get_byte(frm->frame,frm->frame_len,frm->frame_len-2);
    uint8_t CHECKSUM_LOW  = get_byte(frm->frame,frm->frame_len,frm->frame_len-1);
    frm->CHECKSUM = (uint16_t) ((CHECKSUM_HIGH << 8) | CHECKSUM_LOW);
}

static void __recv_frame(){

    // copy frame out of buffer
    DLL_frame frm;
    frm.frame = dll.buf;
    frm.frame_len = dll.buf_size;

    __deframe_dll(&frm);

    // TODO: If not correct receiver flush

    // TODO: lock in a sender once a frame is already in the buffer

    // extract control data

    // printf("FRAGMENT:%u\nFINAL:%d\nPROTOCOL:%d\nMSG_TYPE:%d\nCHECKTYP:%d\nSEQ_NUM:%u\n",
    //     FRAGMENT,
    //     FINAL,
    //     PROTOCOL,
    //     MSG_TYPE,
    //     CHECKTYP,
    //     SEQ_NUM);
        
    // with this information, decide what to do
    // switch(MSG_TYPE){
    //     case MSG:
            // if message, store at end of fragment buffer
            // __store_fragment(frm, FRAGMENT, SEQ_NUM, FINAL);

            // if final frame present, check for a complete packet

            // if returns non-null, use returned vector to reconstruct NET pkt

            // break;
        // case ACK:
            // move sliding window
            // break;
        // case NACK:
            // queue packet resend
            // break;
    // }
}

// TODO: check if sequence numbers match in with others before inserting or discard
// TODO: lock in a sender once one frame is in an discard others
//       -- only allow frames to enter which have lower sequence numbers and lower fragments
//       -- or, higher sequence numbers and higher fragments
// static void __store_fragment(DLL_frame* frm, uint8_t fragment, uint8_t seq, bool final){
    // // TODO: Check for sequence number before performing any append

    // // increase size of frame buffer
    // ++dll.frmbuf_size;
    // dll.frmbuf = (DLL_frame**) realloc(dll.frmbuf,dll.frmbuf_size * sizeof(DLL_frame*));

    // if(fragment == 0x0) { // check if first frame is stored
        // dll.first_present = true;
    // }

    // if(final){ // if final fragment, always store at end
        // dll.frmbuf[dll.frmbuf_size - 1] = *frm;
        // dll.final_present = true;
    // } else { // otherwise iterate through and store pointer in order
        // uint8_t frag_insert_index = 0;
        // for(size_t f = 0; f < dll.frmbuf_size; ++f){
            // DLL_frame frm = dll.frmbuf[f]; 
            // uint8_t buf_fragment = frm.frame[frm.CTRL_HIGH] & 0x7; 
            // // find buffered fragment with higher fragment number than this
            // if(buf_fragment > fragment){
                // frag_insert_index = f;
                // break;
            // }
        // }
        // // now, work backwards and shuffle pointers to make room for insert
        // for(size_t f = dll.frmbuf_size; f <= frag_insert_index; --f){
            // dll.frmbuf[f] = dll.frmbuf[f-1];
        // }
        // dll.frmbuf[frag_insert_index] = *frm;
    // }

    // // if first and final present, set fragment count total for check
// }

// if we know there's a first and a final, all we need check is the length of the buffer
// static bool __check_complete_pkt(){
//     // given that there is a first and a final frame, check for all fragments in sequence
//     size_t total = 0;
//     for(size_t f = 0; f < dll.frmbuf_size - 1; ++f){
//         // get current and next frames
//         DLL_frame cur_frm = dll.frmbuf[f];
//         DLL_frame nxt_frm = dll.frmbuf[f+1];
//         // get fragment numbers
//         uint8_t cur_frag = cur_frm.frame[cur_frm.CTRL_HIGH] & 0x7;
//         uint8_t nxt_frag = nxt_frm.frame[nxt_frm.CTRL_HIGH] & 0x7;

//         if(nxt_frag != cur_frag + 1){ // frame fragments are non-sequential
//             return false;
//         }
//     }
//     return true;
// }

// static NET_packet __reconstruct_pkt(){
//     // iterate through the buffer
//     // on each frame, take the frame length as the length needed to append bytes to the existing NET_packet buffer.
//     NET_packet pkt;
//     pkt.packet = NULL;
//     pkt.pkt_size = 0;
//     for(size_t f = 0; f < pkt.pkt_size; ++f){
//         DLL_frame frm = dll.frmbuf[f];
//         append_bytes(&pkt.packet,&pkt.pkt_size,frm.frame,frm.frame_len);
//     }
//     return pkt;
// }

void service_dll(){

}


// INTERNAL METHODS
// Packet fragmentation methods
static uint8_t __get_num_pkt_fragments(uint8_t pkt_length, uint8_t max_length){
    if(pkt_length == 0){return 1;}

    uint8_t full_pkts = pkt_length / max_length; // number of full packets
    uint8_t frag_pkt_len = pkt_length % max_length; // length of final packet

    uint8_t num_frames = full_pkts; // number of frames is at least full packets
    if(frag_pkt_len > 0) {num_frames++;} // if short packet fragment, add it

    return num_frames;
}

static uint8_t* __get_len_pkt_fragments(uint8_t pkt_length, uint8_t max_length){
    uint8_t num_frames = __get_num_pkt_fragments(pkt_length, max_length);
    uint8_t* frag_lengths = (uint8_t*) malloc(num_frames*sizeof(uint8_t));
    if(frag_lengths != NULL){
        if(num_frames > 1){
            for(int p = 0; p < num_frames; ++p){
                if(p < num_frames - 1){frag_lengths[p] = max_length;} //
                else{
                    if(pkt_length % max_length == 0){frag_lengths[p] = max_length;}
                    else{frag_lengths[p] = pkt_length % max_length;}
                }
            }
        } else { // 1 frame packet length
            // covers edge case of dummy payload: 1 frame, 0 payload
            frag_lengths[0] = pkt_length;
        }
    } else {
        fprintf(stderr,"__get_len_pkt_fragments failed");
        exit(EXIT_FAILURE);
    }

    return frag_lengths;
}

static uint8_t** __get_pkt_fragments(uint8_t* len_fragments, uint8_t num_fragments, uint8_t* pkt, size_t len_pkt){
    uint8_t** fragments = (uint8_t**) malloc(num_fragments*sizeof(uint8_t*));
    if(fragments != NULL){
        size_t start_index = 0;
        for(size_t f = 0; f < num_fragments; ++f){
               size_t len_fragment = len_fragments[f]; 
               fragments[f] = get_bytes_from(pkt,len_pkt,start_index,len_fragment);
               start_index += len_fragment;
        }
    } else {
        fprintf(stderr,"__get_pkt_fragments failed");
        exit(EXIT_FAILURE);
    }
    return fragments;
}

static uint16_t __get_control_subframe(DLL_frame* frm){
    uint16_t ctrl = 0;
    // modify from spec: 3-bit fragment, next bit 
    ctrl |= frm->FRAGMENT << 13; // BBB00000-00000000
    ctrl |= frm->FINAL    << 12; // 000B0000-00000000
    ctrl |= DLL_PROTOCOL << 11; // 0000B000-00000000
    ctrl |= frm->MSG_TYPE << 9;  // 00000BB0-00000000
    ctrl |= DLL_CHECKSUM << 8;  // 0000000B-00000000
    ctrl |= frm->SEQ_NUM;        // 00000000-BBBBBBBB
    return ctrl;
}

static uint16_t __get_checksum_subframe(uint8_t* frame, size_t frame_len){
    switch(DLL_CHECKSUM){
        case 0: return __get_parity(frame, frame_len);
        case 1: return __get_CRC16(frame,frame_len);
    }
}

static uint16_t __get_CRC16(uint8_t* frame, size_t frame_len){
    uint16_t crc = 0xFFFF; // Initial value
    size_t i, j;

    for (i = 0; i < frame_len; ++i) {
        crc ^= (uint16_t)frame[i];
        for (j = 0; j < 8; ++j) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ DLL_CRC16_POLYNOMIAL;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
    
}

static uint16_t __get_parity(uint8_t* frame, size_t frame_len){
    return (uint16_t) 0xC3C3;
}

static void __escape_frame(uint8_t** frame, size_t* frame_len){
    // look through all bytes in frame, checking for unintentional flag/escapes
    uint8_t byte;
    for(size_t b = 1; b < (*frame_len); ++b){ // skip over header; no ESC needed
        byte = (*frame)[b];

        // if flag (0x7E), prepend ESC (0x7D). if ESC (0x7D), prepend ESC (0x7D)
        if(byte == 0x7E || byte == 0x7D){
            insert_byte(frame,frame_len,DLL_ESC_BYTE,b);
            ++b; // skip over new prepended ESC
        }
    }
}