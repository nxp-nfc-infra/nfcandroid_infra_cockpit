/******************************************************************************
 *
 *  Copyright (C) 2011-2012 Broadcom Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

/******************************************************************************
 *
 *  The original Work has been changed by NXP Semiconductors.
 *
 *  Copyright 2013-2014,2022-2023 NXP
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

#include <phNxpConfig.h>
#include <phNxpLog.h>
#include <stdio.h>
#include <sys/stat.h>

#include <list>
#include <string>
#include <vector>

#define IsStringValue 0x80000000
static FILE* sfd = NULL;

static long last_line_start_point = 0;

using namespace ::std;

class CNfcParam : public string {
 public:
  CNfcParam();
  CNfcParam(const char* name, const string& value);
  CNfcParam(const char* name, unsigned long value);
  virtual ~CNfcParam();
  unsigned long numValue() const { return m_numValue; }
  const char* str_value() const { return m_str_value.c_str(); }
  size_t str_len() const { return m_str_value.length(); }

 private:
  string m_str_value;
  unsigned long m_numValue;
};

class CNfcConfig : public vector<const CNfcParam*> {
 public:
  virtual ~CNfcConfig();
  static CNfcConfig& GetInstance();

  const CNfcParam* find(const char* p_name) const;
  void clean();
  void closeScr(void);
  int readScr(const char* name, char* data_buf, scr_cmd* cmd);
  bool pullBackToLast();
  bool pullBackToStart();

 private:
  CNfcConfig();
  void moveFromList();
  void moveToList();
  void add(const CNfcParam* pParam);
  list<const CNfcParam*> m_list;
  bool mValidFile;
  unsigned long m_timeStamp;

  unsigned long state;

  inline bool Is(unsigned long f) { return (state & f) == f; }
  inline void Set(unsigned long f) { state |= f; }
  inline void Reset(unsigned long f) { state &= ~f; }
};

enum { BEGIN_LINE = 1, TOKEN, BEGIN_HEX, END_LINE, READ_LINE_DONE, COMMENT };

static long loop_start_p = 0;
unsigned char loop_running = 0;
unsigned long loop_count = 0;
void set_loop_state(uint8_t state) { loop_running = state; }

void set_loop_count(unsigned long count) { loop_count = count; }

/*******************************************************************************
**
** Function:    isPrintable()
**
** Description: determine if 'c' is printable
**
** Returns:     1, if printable, otherwise 0
**
*******************************************************************************/
inline bool isPrintable(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
         (c >= '0' && c <= '9') || c == '/' || c == '_' || c == '-' || c == '.';
}

/*******************************************************************************
**
** Function:    isDigit()
**
** Description: determine if 'c' is numeral digit
**
** Returns:     true, if numerical digit
**
*******************************************************************************/
inline bool isDigit(char c, int base) {
  if ('0' <= c && c <= '9') return true;
  if (base == 16) {
    if (('A' <= c && c <= 'F') || ('a' <= c && c <= 'f')) return true;
  }
  return false;
}

/*******************************************************************************
**
** Function:    getDigitValue()
**
** Description: return numerical value of a decimal or hex char
**
** Returns:     numerical value if decimal or hex char, otherwise 0
**
*******************************************************************************/
inline int getDigitValue(char c, int base) {
  if ('0' <= c && c <= '9') return c - '0';
  if (base == 16) {
    if ('A' <= c && c <= 'F')
      return c - 'A' + 10;
    else if ('a' <= c && c <= 'f')
      return c - 'a' + 10;
  }
  return 0;
}

/*******************************************************************************
**
** Function:    CNfcConfig::pullBackToLast()
**
** Description: pullback seek to start point of last line
**
** Returns:     TRUE : success, FALSE : fail
**
*******************************************************************************/
bool CNfcConfig::pullBackToLast() {
  int ret_fseek = 0;
  if (sfd == NULL || last_line_start_point == 0) {
    NXPLOG_EXTNS_E("File is not opened!!");
    return FALSE;
  }

  ret_fseek = fseek(sfd, last_line_start_point, SEEK_SET);
  if (ret_fseek == -1) {
    NXPLOG_EXTNS_E("Failed to set file position indicator in %s!!", __func__);
    return FALSE;
  }

  return TRUE;
}

/*******************************************************************************
**
** Function:    CNfcConfig::pullBackToStart()
**
** Description: pullback seek to start point of last line
**
** Returns:     TRUE : success, FALSE : fail
**
*******************************************************************************/
bool CNfcConfig::pullBackToStart() {
  if (sfd == NULL) {
    NXPLOG_EXTNS_E("File is not opened!!");
    return FALSE;
  }

  fseek(sfd, 0, SEEK_SET);

  return TRUE;
}

void CNfcConfig::closeScr(void) {
  fclose(sfd);
  sfd = NULL;
}
int CNfcConfig::readScr(const char* name, char* data_buf, scr_cmd* cmd) {
  int len = 0;
  unsigned long state;
  struct stat buf;
  string token;
  string strValue;
  char c;
  int push_count = 0;
  int bar_count = 0;
  int ret_stat = 0;
  int ret_fread = 0;

  NXPLOG_EXTNS_D("CNfcConfig::readScr");
  state = BEGIN_LINE;

  if (sfd == NULL) {
    /* open config file, read it into a buffer */
    NXPLOG_EXTNS_D("Script file : %s\n", name);
    if ((sfd = fopen(name, "rb")) == NULL) {
      printf("%s Cannot open config file %s\n", __func__, name);
      return -1;
    }
  }

  ret_stat = stat(name, &buf);
  if (ret_stat == -1) {
    NXPLOG_EXTNS_E("Failed to retrive information about file in %s!!",
                   __func__);
    return -1;
  }
  token.erase();

  last_line_start_point = ftell(sfd);
  NXPLOG_EXTNS_D("last_line_start_point : %ld", last_line_start_point);

  while ((state & 0xff) != READ_LINE_DONE && !feof(sfd) &&
         fread(&c, 1, 1, sfd) == 1) {
    //        NXPLOG_EXTNS_D("SCR Read, c : %c, state : %ld", c, state & 0xff);
    switch (state & 0xff) {
      case BEGIN_LINE:
        if ((c == 's') || (c == 'l') || (c == 'e') || (c == 't') ||
            (c == 'r') || (c == 'i') || (c == 'p')) {
          state = TOKEN;
          token.push_back(c);
        } else if (c == '\n' || c == '\r' || c == 0x0A) {
          *cmd = NONE;
          state = READ_LINE_DONE;
        } else if (c == '/') {
          *cmd = NONE;
          ret_fread = fread(&c, 1, 1, sfd);
          if (ret_fread == 0) {
            NXPLOG_EXTNS_E("Failed to read file in %s", __func__);
            return 0;
          }
          if (c == '*') {
            state = COMMENT;
          } else if (c == '\n' || c == '\r' || c == 0x0A) {
            state = READ_LINE_DONE;
          } else {
            state = END_LINE;
          }
        } else if (c == '#') {
          *cmd = NONE;
          state = END_LINE;
        } else {
          *cmd = NONE;
          state = BEGIN_LINE;
        }
        break;
      case TOKEN:
        if (c == ' ') {
          state = BEGIN_HEX;
          strValue.erase();
          if (token == "send") {
            *cmd = SEND;
          } else if (token == "sleep") {
            *cmd = SLEEP;
          } else if (token == "trigger") {
            *cmd = TRIGGER;
          } else if (token == "loop") {
            *cmd = LOOPS;
          } else if (token == "end") {
            *cmd = END;
            state = END_LINE;
          } else if (token == "reset") {
            *cmd = RESET;
          } else if (token == "interval") {
            *cmd = INTERVAL;
          } else if (token == "resetspi") {
            *cmd = RESETSPI;
          } else if (token == "pwrreq") {
            *cmd = PWRREQ;
          } else if (token == "sendspi") {
            *cmd = SENDSPI;
          } else if (token == "stop") {
            *cmd = STOP;
          } else {
            *cmd = NONE;
            state = END_LINE;
          }
        } else if (c == '\n' || c == '\r' || c == 0x0A) {
          if (token == "end") {
            *cmd = END;
            state = READ_LINE_DONE;
          } else {
            *cmd = NONE;
            state = READ_LINE_DONE;
          }
        } else {
          token.push_back(c);
        }
        break;

      case BEGIN_HEX:
        if (isDigit(c, 16) || c == '|') {
          push_count++;
          strValue.push_back(c);
          if (c == '|') bar_count++;
        } else if (c == '\n' || c == '\r' || c == 0x0A) {
          if (push_count > 0) {
            if ((bar_count == 0) && (push_count % 2))
              strValue.insert(strValue.begin(), '0');
          } else {
            *cmd = NONE;
          }
          state = READ_LINE_DONE;
        } else {
          state = END_LINE;
        }

        break;

      case END_LINE:
        if (c == '\n' || c == '\r' || c == 0x0A) {
          if ((*cmd != END) && (*cmd != NONE)) {
            //                    if(push_count%2)
            //                        strValue.insert(strValue.begin(), '0');
          }
          state = READ_LINE_DONE;
        }
        break;
      case COMMENT:
        if (c == '*') {
          ret_fread = fread(&c, 1, 1, sfd);
          if (ret_fread == 0) {
            NXPLOG_EXTNS_E("Failed to read file in %s", __func__);
            return 0;
          }
          if (c == '/') {
            *cmd = NONE;
            state = END_LINE;
          }
        }
        break;
      default:
        NXPLOG_EXTNS_E("Invalid State in reading script !!");
        *cmd = NONE;
        state = READ_LINE_DONE;
        break;
    }
  }

  strncpy(data_buf, strValue.c_str(), strValue.size());

  if (RESETSPI <= *cmd && *cmd <= SENDSPI) {
    NXPLOG_EXTNS_W(
        "----------------------------------------SCR Read, cmd : %d, data : "
        "%s\n",
        *cmd, data_buf);
  } else if (*cmd != NONE) {
    NXPLOG_EXTNS_W("SCR Read, cmd : %d, data : %s\n", *cmd, data_buf);
  }

  if (state == READ_LINE_DONE) {
    long current_read_point;
    current_read_point = ftell(sfd);

    if (*cmd == LOOPS) {
      loop_start_p = ftell(sfd);
      set_loop_state(true);
    }

    if ((*cmd == END) && (loop_running == true)) {
      if (loop_count > 1) {
        fseek(sfd, loop_start_p, SEEK_SET);
        loop_count--;
        current_read_point = ftell(sfd);
      } else
        set_loop_state(false);
    }

    len = strValue.size() / 2;
    if (strValue.size() % 2) len++;
    return len;
  }

  *cmd = EOS;
  fclose(sfd);
  return 0;
}

/*******************************************************************************
**
** Function:    CNfcConfig::CNfcConfig()
**
** Description: class constructor
**
** Returns:     none
**
*******************************************************************************/
CNfcConfig::CNfcConfig() : mValidFile(true), m_timeStamp(0), state(0) {}

/*******************************************************************************
**
** Function:    CNfcConfig::~CNfcConfig()
**
** Description: class destructor
**
** Returns:     none
**
*******************************************************************************/
CNfcConfig::~CNfcConfig() {}

/*******************************************************************************
**
** Function:    CNfcConfig::GetInstance()
**
** Description: get class singleton object
**
** Returns:     none
**
*******************************************************************************/
CNfcConfig& CNfcConfig::GetInstance() {
  static CNfcConfig theInstance;
  return theInstance;
}

/*******************************************************************************
**
** Function:    CNfcConfig::clean()
**
** Description: reset the setting array
**
** Returns:     none
**
*******************************************************************************/
void CNfcConfig::clean() {
  if (size() == 0) return;

  for (iterator it = begin(), itEnd = end(); it != itEnd; ++it) delete *it;
  clear();
}

/*******************************************************************************
**
** Function:    CNfcConfig::Add()
**
** Description: add a setting object to the list
**
** Returns:     none
**
*******************************************************************************/
void CNfcConfig::add(const CNfcParam* pParam) {
  if (m_list.size() == 0) {
    m_list.push_back(pParam);
    return;
  }
  for (list<const CNfcParam*>::iterator it = m_list.begin(),
                                        itEnd = m_list.end();
       it != itEnd; ++it) {
    if (**it < pParam->c_str()) continue;
    m_list.insert(it, pParam);
    return;
  }
  m_list.push_back(pParam);
}

/*******************************************************************************
**
** Function:    CNfcConfig::moveFromList()
**
** Description: move the setting object from list to array
**
** Returns:     none
**
*******************************************************************************/
void CNfcConfig::moveFromList() {
  if (m_list.size() == 0) return;

  for (list<const CNfcParam*>::iterator it = m_list.begin(),
                                        itEnd = m_list.end();
       it != itEnd; ++it)
    push_back(*it);
  m_list.clear();
}

/*******************************************************************************
**
** Function:    CNfcConfig::moveToList()
**
** Description: move the setting object from array to list
**
** Returns:     none
**
*******************************************************************************/
void CNfcConfig::moveToList() {
  if (m_list.size() != 0) m_list.clear();

  for (iterator it = begin(), itEnd = end(); it != itEnd; ++it)
    m_list.push_back(*it);
  clear();
}

/*******************************************************************************
**
** Function:    CNfcParam::CNfcParam()
**
** Description: class constructor
**
** Returns:     none
**
*******************************************************************************/
CNfcParam::CNfcParam() : m_numValue(0) {}

/*******************************************************************************
**
** Function:    CNfcParam::~CNfcParam()
**
** Description: class destructor
**
** Returns:     none
**
*******************************************************************************/
CNfcParam::~CNfcParam() {}

/*******************************************************************************
**
** Function:    CNfcParam::CNfcParam()
**
** Description: class copy constructor
**
** Returns:     none
**
*******************************************************************************/
CNfcParam::CNfcParam(const char* name, const string& value)
    : string(name), m_str_value(value), m_numValue(0) {}

/*******************************************************************************
**
** Function:    CNfcParam::CNfcParam()
**
** Description: class copy constructor
**
** Returns:     none
**
*******************************************************************************/
CNfcParam::CNfcParam(const char* name, unsigned long value)
    : string(name), m_numValue(value) {}

extern "C" void closeScrfd(void) {
  CNfcConfig& rConfig = CNfcConfig::GetInstance();
  rConfig.closeScr();
}

extern "C" int readScrline(const char* name, char* data_buf, scr_cmd* cmd) {
  CNfcConfig& rConfig = CNfcConfig::GetInstance();
  //    NXPLOG_EXTNS_E("readScrline");
  return rConfig.readScr(name, data_buf, cmd);
}

extern "C" bool pullBackScrline() {
  CNfcConfig& rConfig = CNfcConfig::GetInstance();
  NXPLOG_EXTNS_D("pullBackScrline");
  return rConfig.pullBackToLast();
}

extern "C" bool pullBackScrstart() {
  CNfcConfig& rConfig = CNfcConfig::GetInstance();
  NXPLOG_EXTNS_D("pullBackScrstart");
  return rConfig.pullBackToStart();
}

/*******************************************************************************
**
** Function:    resetConfig
**
** Description: reset settings array
**
** Returns:     none
**
*******************************************************************************/
extern "C" void resetNxpConfig()

{
  CNfcConfig& rConfig = CNfcConfig::GetInstance();

  rConfig.clean();
}
