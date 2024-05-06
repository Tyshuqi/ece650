#include <iostream>
#include <pqxx/pqxx>
#include <fstream>
#include <sstream>
#include <vector>

#include "exerciser.h"

using namespace std;
using namespace pqxx;

// Function to split a string by a delimiter into a vector
vector<string> split(const string &s, char delimiter)
{
  vector<string> tokens;
  string token;
  istringstream tokenStream(s);
  while (getline(tokenStream, token, delimiter))
  {
    tokens.push_back(token);
  }
  return tokens;
}

// Load player
void loadPlayerTable(connection *C, const string &filePath)
{
  ifstream file(filePath);
  if (!file)
  {
    cerr << "Cannot open file: " << filePath << endl;
    return; // Or handle the error appropriately
  }
  string line;

  pqxx::work W(*C);

  while (getline(file, line))
  {
    vector<string> tokens = split(line, ' '); // Assuming CSV format

    // Construct the SQL insert statement
    string sql = "INSERT INTO PLAYER (TEAM_ID, UNIFORM_NUM, FIRST_NAME, LAST_NAME, MPG, PPG, RPG, APG, SPG, BPG) VALUES (";
    //sql += tokens[0] + ", ";
    sql += W.quote(tokens[1]) + ", ";
    sql += W.quote(tokens[2]) + ", ";
    sql += W.quote(tokens[3]) + ", ";
    sql += W.quote(tokens[4]) + ", ";
    sql += W.quote(tokens[5]) + ", ";
    sql += W.quote(tokens[6]) + ", ";
    sql += W.quote(tokens[7]) + ", ";
    sql += W.quote(tokens[8]) + ", ";
    sql += W.quote(tokens[9]) + ", ";
    sql += W.quote(tokens[10]);
    sql += ");";

    // Execute the SQL statement
    W.exec(sql);
  }
  // Commit the transaction
  W.commit();
}

void loadStateTable(connection *C, const string &filePath)
{
  ifstream file(filePath);
  if (!file)
  {
    cerr << "Cannot open file: " << filePath << endl;
    return; // Or handle the error appropriately
  }
  string line;

  pqxx::work W(*C);

  while (getline(file, line))
  {
    vector<string> tokens = split(line, ' ');

    // Assuming the file contains only the NAME column for each STATE
    string sql = "INSERT INTO STATE (NAME) VALUES (";
    // sql += tokens[0] + ", ";
    sql += W.quote(tokens[1]); // Assuming first token is the state's NAME
    sql += ");";

    W.exec(sql);
  }
  W.commit();
  cout << "Data loaded into STATE table successfully." << endl;
}

void loadColorTable(connection *C, const string &filePath)
{
  ifstream file(filePath);
  if (!file)
  {
    cerr << "Cannot open file: " << filePath << endl;
    return; // Or handle the error appropriately
  }
  string line;

  pqxx::work W(*C);

  while (getline(file, line))
  {
    vector<string> tokens = split(line, ' ');

    string sql = "INSERT INTO COLOR (NAME) VALUES (";
    //sql += tokens[0] + ", ";
    sql += W.quote(tokens[1]);
    sql += ");";

    W.exec(sql);
  }
  W.commit();
  cout << "Data loaded into COLOR table successfully." << endl;
}

void loadTeamTable(connection *C, const string &filePath)
{
  ifstream file(filePath);
  if (!file)
  {
    cerr << "Cannot open file: " << filePath << endl;
    return; // Or handle the error appropriately
  }
  string line;

  pqxx::work W(*C);

  while (getline(file, line))
  {
    vector<string> tokens = split(line, ' ');

    // Assuming the text file is formatted as: TEAM_ID,NAME,STATE_ID,COLOR_ID,WINS,LOSSES
    string sql = "INSERT INTO TEAM (NAME, STATE_ID, COLOR_ID, WINS, LOSSES) VALUES (";
   // sql += tokens[0] + ", ";
    sql += W.quote(tokens[1]) + ", "; // NAME
    sql += tokens[2] + ", ";          // STATE_ID
    sql += tokens[3] + ", ";          // COLOR_ID
    sql += tokens[4] + ", ";          // WINS
    sql += tokens[5];                 // LOSSES
    sql += ");";

    W.exec(sql);
  }
  W.commit();
  cout << "Data loaded into TEAM table successfully." << endl;
}

int main(int argc, char *argv[])
{

  // Allocate & initialize a Postgres connection object
  connection *C;

  try
  {
    // Establish a connection to the database
    // Parameters: database name, user name, user password
    C = new connection("dbname=ACC_BBALL user=postgres password=passw0rd");
    if (C->is_open())
    {
      cout << "Opened database successfully: " << C->dbname() << endl; 
    }
    else
    {
      cout << "Can't open database" << endl;
      return 1;
    }
  }
  catch (const std::exception &e)
  {
    cerr << e.what() << std::endl;
    return 1;
  }

  // TODO: create PLAYER, TEAM, STATE, and COLOR tables in the ACC_BBALL database
  pqxx::work W(*C);
  std::string createPlayerTable = R"(
    DROP TABLE IF EXISTS PLAYER CASCADE;
            CREATE TABLE PLAYER(
                PLAYER_ID SERIAL PRIMARY KEY,
                TEAM_ID INT REFERENCES TEAM(TEAM_ID),
                UNIFORM_NUM INT,
                FIRST_NAME VARCHAR(50),
                LAST_NAME VARCHAR(50),
                MPG INT,  
                PPG INT,  
                RPG INT,  
                APG INT,  
                SPG NUMERIC(3,1),  
                BPG NUMERIC(3,1)   
            );
        )";

  std::string createTeamTable = R"(
    DROP TABLE IF EXISTS TEAM CASCADE;
            CREATE TABLE TEAM(
                TEAM_ID SERIAL PRIMARY KEY,
                NAME VARCHAR(50),
                STATE_ID INT,
                COLOR_ID INT,
                WINS INT,
                LOSSES INT
            );
        )";

  std::string createStateTable = R"(
    DROP TABLE IF EXISTS STATE CASCADE;
            CREATE TABLE STATE(
                STATE_ID SERIAL PRIMARY KEY,
                NAME VARCHAR(50)
            );
        )";

  std::string createColorTable = R"(
    DROP TABLE IF EXISTS COLOR CASCADE;
            CREATE TABLE COLOR(
                COLOR_ID SERIAL PRIMARY KEY,
                NAME VARCHAR(50)
            );
        )";

  W.exec(createStateTable);
  cout << "Table STATE created successfully" << endl;

  W.exec(createColorTable);
  cout << "Table COLOR created successfully" << endl;

  W.exec(createTeamTable);
  cout << "Table TEAM created successfully" << endl;

  W.exec(createPlayerTable);
  cout << "Table PLAYER created successfully" << endl;

  W.commit();

  //      load each table with rows from the provided source txt files

  if (argc > 4)
  {
    loadStateTable(C, argv[3]); // argv[3] should be the path to your STATE data file
    cout << "Data loaded into STATE table successfully." << endl;

    loadColorTable(C, argv[4]); // argv[4] should be the path to your COLOR data file
    cout << "Data loaded into COLOR table successfully." << endl;

    loadTeamTable(C, argv[2]); // argv[2] should be the path to your TEAM data file
    cout << "Data loaded into TEAM table successfully." << endl;

    loadPlayerTable(C, argv[1]); // argv[1] should be the path to your PLAYER data file
    cout << "Data loaded into PLAYER table successfully." << endl;
  }
  else
  {
    cerr << "Not enough file paths provided. Expecting 4 files for PLAYER, TEAM, STATE, and COLOR." << endl;
    return 1;
  }

  exercise(C);

  // Close database connection
  C->disconnect();

  return 0;
}
