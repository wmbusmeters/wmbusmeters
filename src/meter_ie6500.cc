/*
 Copyright (C) 2020 Fredrik Öhrström

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include"dvparser.h"
#include"meters.h"
#include"meters_common_implementation.h"
#include"wmbus.h"
#include"wmbus_utils.h"
#include"string.h"
#define INFO_CODE_SMOKE 0x0001

struct MeterIE6500 : public virtual SmokeDetector, public virtual MeterCommonImplementation
{
    MeterIE6500(MeterInfo &mi);

    string status();
    bool smokeDetected();
    string messageDate();
    string commissionDate();
    string lastSounderTestDate();
    string lastAlarmDate();
    string smokeAlarmCounter();
    string removedCounter();
    string totalRemoveDuration();
    string lastRemoveDate();
    string testButtonCounter();
    string testButtonLastDate();
    string removed();
    string lowBattery();
    string installationCompleted();
    string enviromentChanged();
    string obstacleDetected();
    string coveringDetected();

private:

    void processContent(Telegram *t);

    private:

    uint16_t info_codes_ {};
    bool error_ {};
    string message_date_ {};
    uint16_t smoke_alarm_counter_ {};
    string last_alarm_date_ {};
    uint32_t total_remove_duration_ {};
    string last_remove_date_ {};
    string test_button_last_date_ {};
    bool obstacle_detected_ {};
    bool covering_detected_ {};
    bool installation_completed_ {};
    bool enviroment_changed_ {};
    bool unable_to_complete_scan_ {};
    bool low_battery_ {};
    bool removed_ {};
    bool end_of_life_ {};
    uint16_t removed_counter_ {};
    uint16_t test_button_counter_ {};
    int status_ {};
};

MeterIE6500::MeterIE6500(MeterInfo &mi) :
    MeterCommonImplementation(mi, MeterType::IE6500)
{
    setExpectedTPLSecurityMode(TPLSecurityMode::AES_CBC_IV);

    addLinkMode(LinkMode::C1);
            
    addPrint("message_date", Quantity::Text,
             [&](){ return messageDate(); },
             "Date of message.",
             true, true); 
    addPrint("last_alarm_date", Quantity::Text,
             [&](){ return lastAlarmDate(); },
             "Date of last alarm.",
             true, true);
    addPrint("smoke_alarm_counter", Quantity::Text,
             [&](){ return smokeAlarmCounter(); },
             "smoke alarm counter",
             true, true);             
    addPrint("total_remove_duration", Quantity::Text,
             [&](){ return totalRemoveDuration(); },
             "Total time removed.",
             true, true);
    addPrint("last_remove_date", Quantity::Text,
             [&](){ return lastRemoveDate(); },
             "Date of last removal.",
             true, true);
    addPrint("removed_counter", Quantity::Text,
             [&](){ return removedCounter(); },
             "removed counter",
             true, true);
    addPrint("test_button_last_date", Quantity::Text,
             [&](){ return testButtonLastDate(); },
             "Date of last test button press.",
             true, true);
    addPrint("test_button_counter", Quantity::Text,
             [&](){ return testButtonCounter(); },
             "test button counter",
             true, true);
    addPrint("obstacle_detected", Quantity::Text,
             [&](){ return obstacleDetected(); },
             "Obstacle detected.",
             true, true);
    addPrint("covering_detected", Quantity::Text,
             [&](){ return coveringDetected(); },
             "Covering detected.",
             true, true);
    addPrint("installation_completed", Quantity::Text,
             [&](){ return installationCompleted(); },
             "installation completed.",
             true, true);
    addPrint("enviroment_changed", Quantity::Text,
             [&](){ return enviromentChanged(); },
             "enviroment changed.",
             true, true);
    addPrint("low_battery", Quantity::Text,
             [&](){ return lowBattery(); },
             "low battery",
             true, true);
    addPrint("removed", Quantity::Text,
             [&](){ return removed(); },
             "true if alarm was removed from mounting plate",
             true, true);
    addPrint("status", Quantity::Text,
             [&](){ return status(); },
             "status; no error, error or RTC invalid",
             true, true);




}

shared_ptr<SmokeDetector> createIE6500(MeterInfo &mi)
{
    return shared_ptr<SmokeDetector>(new MeterIE6500(mi));
}

bool MeterIE6500::smokeDetected()
{
    return 0;
}

void MeterIE6500::processContent(Telegram *t)
{
    int offset;
    
    uchar sts = t->tpl_sts;
    if ((sts & 0x03) == 0x00)
    {
        status_ = 0;
    }
    else if ((sts & 0x40) == 0x40)
    {
        status_ = 2;
    }  
    else if ((sts & 0x03) == 0x02)
    {
        status_ = 1;
    }
    else
    {
    	printf("status error");
    }   
    
    struct tm datetime;
    extractDVdate(&t->values, "046D", &offset, &datetime);
    message_date_ = strdatetime(&datetime);
    t->addMoreExplanation(offset, " message date (%zu)", message_date_.c_str());
      
    extractDVdate(&t->values, "82506C", &offset, &datetime);
    last_alarm_date_ = strdatetime(&datetime);
    t->addMoreExplanation(offset, " last alarm date (%zu)", last_alarm_date_.c_str());
  
    extractDVuint16(&t->values, "8250FD61", &offset, &smoke_alarm_counter_);
    t->addMoreExplanation(offset, " smoke alarm counter (%zu)", smoke_alarm_counter_);

    extractDVuint16(&t->values, "8260FD61", &offset, &removed_counter_);
    t->addMoreExplanation(offset, " removed counter (%zu)", removed_counter_);

    extractDVuint16(&t->values, "8270FD61", &offset, &test_button_counter_);
    t->addMoreExplanation(offset, " test button counter (%zu)", test_button_counter_);

    extractDVuint24(&t->values, "8360FD31", &offset, &total_remove_duration_);
    t->addMoreExplanation(offset, " total remove duration (%zu)", total_remove_duration_);
 
    extractDVdate(&t->values, "82606C", &offset, &datetime);
    last_remove_date_ = strdatetime(&datetime);
    t->addMoreExplanation(offset, " last remove date (%zu)", last_remove_date_.c_str());

    extractDVdate(&t->values, "82706C", &offset, &datetime);
    test_button_last_date_ = strdatetime(&datetime);
    t->addMoreExplanation(offset, " test button last date (%zu)", test_button_last_date_.c_str());
    
    vector<uchar> content;
    t->extractPayload(&content);
    uchar head1 = content[30];
    uchar head2 = content[31];
    uchar head3 = content[32];
    uchar head4 = content[33];

 
    if ((head4 & 0x01) == 0x01)
    {
        obstacle_detected_ = 1;
    }
    if ((head4 & 0x02) == 0x02)
    {
        covering_detected_ = 1;
    }   
    if ((head3 & 0x01) == 0x01)
    {
        installation_completed_ = 1;
    }  
    if ((head3 & 0x02) == 0x02)
    {
        enviroment_changed_ = 1;
    }     
     if ((head2 & 0x10) == 0x10)
     {
         low_battery_ = 1; 
     }  
     if ((head1 & 0x40) == 0x40)
     {
         removed_ = 1;
     }                        
}

string MeterIE6500::status()
{
	if(status_ == 0)
	{
		return "no error";
	}
	else
	{
		return "error";
	}
}

string MeterIE6500::messageDate()
{
	return message_date_;
}
string MeterIE6500::lastAlarmDate()
{
	return last_alarm_date_;
}
string MeterIE6500::totalRemoveDuration()
{
	return to_string(total_remove_duration_) + " minutes";
}
string MeterIE6500::smokeAlarmCounter()
{
	return to_string(smoke_alarm_counter_);
}
string MeterIE6500::testButtonCounter()
{
	return to_string(test_button_counter_);
}
string MeterIE6500::removedCounter()
{
	return to_string(removed_counter_);
}
string MeterIE6500::lastRemoveDate()
{
	return last_remove_date_;
}
string MeterIE6500::testButtonLastDate()
{
	return test_button_last_date_;
}

string MeterIE6500::obstacleDetected()
{
	if(obstacle_detected_)
	{
		return "obstacle detected";
	}
	else
	{
		return "no obstacle detected";
	}
}
string MeterIE6500::coveringDetected()
{
	if(covering_detected_)
	{
		return "covering detected";
	}
	else
	{
		return "no covering detected";
	}
}
string MeterIE6500::enviromentChanged()
{
	if(enviroment_changed_)
	{
		return "enviroment changed since install";
	}
	else
	{
		return "no enviroment change";
	}
}
string MeterIE6500::installationCompleted()
{
	if(installation_completed_)
	{
		return "installation completed";
	}
	else
	{
		return "installation not completed";
	}
}

string MeterIE6500::lowBattery()
{
	if(low_battery_)
	{
		return "battery low";
	}
	else
	{
		return "battery ok";
	}
}

string MeterIE6500::removed()
{
	if(removed_)
	{
		return "alarm removed from mounting plate";
	}
	else
	{
		return "not removed from plate";
	}
}
