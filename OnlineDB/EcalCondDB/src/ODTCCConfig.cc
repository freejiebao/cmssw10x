#include <stdexcept>
#include <cstdlib>
#include <string>
#include "OnlineDB/Oracle/interface/Oracle.h"

#include "OnlineDB/EcalCondDB/interface/ODTCCConfig.h"

using namespace std;
using namespace oracle::occi;

ODTCCConfig::ODTCCConfig()
{
  m_env = nullptr;
  m_conn = nullptr;
  m_writeStmt = nullptr;
  m_readStmt = nullptr;
  m_config_tag="";

   m_ID=0;
   clear();
   m_size=0; 
}

void ODTCCConfig::clear(){
  
  m_tcc_file="";
  m_lut_file="";
  m_slb_file="";
  m_test_url="";
  m_ntest=0;
}


ODTCCConfig::~ODTCCConfig()
{
}

int ODTCCConfig::fetchNextId()  noexcept(false) {

  int result=0;
  try {
    this->checkConnection();

    m_readStmt = m_conn->createStatement(); 
    m_readStmt->setSQL("select ecal_tcc_config_sq.NextVal from dual");
    ResultSet* rset = m_readStmt->executeQuery();
    while (rset->next ()){
      result= rset->getInt(1);
    }
    m_conn->terminateStatement(m_readStmt);
    return result; 

  } catch (SQLException &e) {
    throw(std::runtime_error(std::string("ODTCCConfig::fetchNextId():  ")+getOraMessage(&e)));
  }

}


void ODTCCConfig::setParameters(const std::map<string,string>& my_keys_map){

  // parses the result of the XML parser that is a map of
  // string string with variable name variable value

  for( std::map<std::string, std::string >::const_iterator ci=
         my_keys_map.begin(); ci!=my_keys_map.end(); ci++ ) {

    if(ci->first==  "TCC_CONFIGURATION_ID") setConfigTag(ci->second);
    if(ci->first==  "N_TESTPATTERNS_TO_LOAD") setNTestPatternsToLoad(atoi(ci->second.c_str()));
    if(ci->first==  "LUT_CONFIGURATION_FILE") setLUTConfigurationFile(ci->second );
    if(ci->first==  "CONFIGURATION_FILE") setTCCConfigurationFile(ci->second );
    if(ci->first==  "SLB_CONFIGURATION_FILE") setSLBConfigurationFile(ci->second );
    if(ci->first==  "TESTPATTERNFILE_URL") setTestPatternFileUrl(ci->second );

  }

}



void ODTCCConfig::prepareWrite()
  noexcept(false)
{
  this->checkConnection();

  int next_id=fetchNextId();

  try {
    m_writeStmt = m_conn->createStatement();
    m_writeStmt->setSQL("INSERT INTO ECAL_TCC_CONFIGURATION (tcc_configuration_id, tcc_tag, "
			"Configuration_file, LUT_CONFIGURATION_FILE, SLB_CONFIGURATION_FILE, "
			"TESTPATTERNFILE_URL , N_TESTPATTERNS_TO_LOAD, "
			"tcc_configuration, lut_configuration, slb_configuration ) "
                        "VALUES (:1, :2, :3, :4, :5, :6, :7, :8 , :9, :10)");
    m_writeStmt->setInt(1, next_id);
    m_writeStmt->setString(2, getConfigTag());
    m_writeStmt->setString(3, getTCCConfigurationFile());
    m_writeStmt->setString(4, getLUTConfigurationFile());
    m_writeStmt->setString(5, getSLBConfigurationFile());
    m_writeStmt->setString(6, getTestPatternFileUrl());
    m_writeStmt->setInt(7, getNTestPatternsToLoad());
    // and now the clobs
    oracle::occi::Clob clob1(m_conn);
    clob1.setEmpty();
    m_writeStmt->setClob(8,clob1);

    oracle::occi::Clob clob2(m_conn);
    clob2.setEmpty();
    m_writeStmt->setClob(9,clob2);

    oracle::occi::Clob clob3(m_conn);
    clob3.setEmpty();
    m_writeStmt->setClob(10,clob3);

    m_writeStmt->executeUpdate ();
    m_ID=next_id; 

    m_conn->terminateStatement(m_writeStmt);
    std::cout<<"TCC 3 empty Clobs inserted into CONFIGURATION with id="<<next_id<<std::endl;

    // now we read and update it 
    m_writeStmt = m_conn->createStatement(); 
    m_writeStmt->setSQL ("SELECT tcc_configuration, lut_configuration, slb_configuration FROM ECAL_TCC_CONFIGURATION WHERE"
			 " tcc_configuration_id=:1 FOR UPDATE");

    std::cout<<"updating the clobs 0"<<std::endl;

    
  } catch (SQLException &e) {
    throw(std::runtime_error(std::string("ODTCCConfig::prepareWrite():  ")+getOraMessage(&e)));
  }

  std::cout<<"updating the clob 1 "<<std::endl;
  
}

void ODTCCConfig::writeDB()
  noexcept(false)
{

  std::cout<<"updating the clob 2"<<std::endl;

  try {

    m_writeStmt->setInt(1, m_ID);
    ResultSet* rset = m_writeStmt->executeQuery();

    while (rset->next ())
      {
        oracle::occi::Clob clob1 = rset->getClob (1);
        oracle::occi::Clob clob2 = rset->getClob (2);
        oracle::occi::Clob clob3 = rset->getClob (3);
        cout << "Opening the clob in read write mode" << endl;
	cout << "Populating the clobs" << endl;
	populateClob (clob1, getTCCConfigurationFile(), m_size);
	populateClob (clob2, getLUTConfigurationFile(), m_size);
	populateClob (clob3, getSLBConfigurationFile(), m_size);
       
      }

    m_writeStmt->executeUpdate();
    m_writeStmt->closeResultSet (rset);

  } catch (SQLException &e) {
    throw(std::runtime_error(std::string("ODTCCConfig::writeDB():  ")+getOraMessage(&e)));
  }
  // Now get the ID
  if (!this->fetchID()) {
    throw(std::runtime_error("ODTCCConfig::writeDB:  Failed to write"));
  }


}






void ODTCCConfig::fetchData(ODTCCConfig * result)
  noexcept(false)
{
  this->checkConnection();
  result->clear();
  if(result->getId()==0 && (result->getConfigTag()=="") ){
    throw(std::runtime_error("ODTCCConfig::fetchData(): no Id defined for this ODTCCConfig "));
  }

  try {

    m_readStmt->setSQL("SELECT * "
		       "FROM ECAL_TCC_CONFIGURATION d "
		       " where (tcc_configuration_id = :1 or tcc_tag=:2 )" );
    m_readStmt->setInt(1, result->getId());
    m_readStmt->setString(2, result->getConfigTag());
    ResultSet* rset = m_readStmt->executeQuery();

    rset->next();
    // the first is the id 
    result->setId(rset->getInt(1));
    result->setConfigTag(getOraString(rset,2));

    result->setTCCConfigurationFile(getOraString(rset,3));
    result->setLUTConfigurationFile(getOraString(rset,4));
    result->setSLBConfigurationFile(getOraString(rset,5));
    result->setTestPatternFileUrl(getOraString(rset,6));
    result->setNTestPatternsToLoad(rset->getInt(7));
    //

    Clob clob1 = rset->getClob (8);
    cout << "Opening the clob in Read only mode" << endl;
    clob1.open (OCCI_LOB_READONLY);
    int clobLength=clob1.length ();
    cout << "Length of the clob1 is: " << clobLength << endl;
    unsigned char* buffer = readClob (clob1, clobLength);
    clob1.close ();
    cout<< "the clob buffer is:"<<endl;  
    for (int i = 0; i < clobLength; ++i)
      cout << (char) buffer[i];
    cout << endl;
    result->setTCCClob(buffer );

    Clob clob2 = rset->getClob (9);
    cout << "Opening the clob in Read only mode" << endl;
    clob2.open (OCCI_LOB_READONLY);
    clobLength=clob2.length ();
    cout << "Length of the clob2 is: " << clobLength << endl;
    unsigned char* buffer2 = readClob (clob2, clobLength);
    clob2.close ();
    cout<< "the clob buffer is:"<<endl;  
    for (int i = 0; i < clobLength; ++i)
      cout << (char) buffer2[i];
    cout << endl;
    result->setLUTClob(buffer2 );

    Clob clob3 = rset->getClob (10);
    cout << "Opening the clob in Read only mode" << endl;
    clob3.open (OCCI_LOB_READONLY);
    clobLength=clob3.length ();
    cout << "Length of the clob3 is: " << clobLength << endl;
    unsigned char* buffer3 = readClob (clob3, clobLength);
    clob3.close ();
    cout<< "the clob buffer is:"<<endl;  
    for (int i = 0; i < clobLength; ++i)
      cout << (char) buffer3[i];
    cout << endl;
    result->setSLBClob(buffer3 );

  } catch (SQLException &e) {
    throw(std::runtime_error(std::string("ODTCCConfig::fetchData():  ")+getOraMessage(&e)));
  }
}



int ODTCCConfig::fetchID()    noexcept(false)
{
  if (m_ID!=0) {
    return m_ID;
  }

  this->checkConnection();

  try {
    Statement* stmt = m_conn->createStatement();
    stmt->setSQL("SELECT tcc_configuration_id FROM ecal_tcc_configuration "
                 "WHERE  tcc_tag=:tcc_tag "
		 );

    stmt->setString(1, getConfigTag() );

    ResultSet* rset = stmt->executeQuery();

    if (rset->next()) {
      m_ID = rset->getInt(1);
    } else {
      m_ID = 0;
    }
    m_conn->terminateStatement(stmt);
  } catch (SQLException &e) {
    throw(std::runtime_error(std::string("ODTCCConfig::fetchID:  ")+getOraMessage(&e)));
  }

    return m_ID;
}
