#include "query_funcs.h"

void add_player(connection *C, int team_id, int jersey_num, string first_name, string last_name,
                int mpg, int ppg, int rpg, int apg, double spg, double bpg)
{
  pqxx::work W(*C);

  // Construct the SQL insert statement
  string sql = "INSERT INTO PLAYER (TEAM_ID, UNIFORM_NUM, FIRST_NAME, LAST_NAME, MPG, PPG, RPG, APG, SPG, BPG) VALUES (";
  // sql += tokens[0] + ", ";
  sql += W.quote(team_id) + ", ";
  sql += W.quote(jersey_num) + ", ";
  sql += W.quote(first_name) + ", ";
  sql += W.quote(last_name) + ", ";
  sql += W.quote(mpg) + ", ";
  sql += W.quote(ppg) + ", ";
  sql += W.quote(rpg) + ", ";
  sql += W.quote(apg) + ", ";
  sql += W.quote(spg) + ", ";
  sql += W.quote(bpg);
  sql += ");";

  // Execute the SQL statement
  W.exec(sql);

  // Commit the transaction
  W.commit();
}

void add_team(connection *C, string name, int state_id, int color_id, int wins, int losses)
{
  pqxx::work W(*C);

  // Assuming the text file is formatted as: TEAM_ID,NAME,STATE_ID,COLOR_ID,WINS,LOSSES
  string sql = "INSERT INTO TEAM (NAME, STATE_ID, COLOR_ID, WINS, LOSSES) VALUES (";
  sql += W.quote(name) + ", ";       // NAME
  sql += to_string(state_id) + ", "; // STATE_ID
  sql += to_string(color_id) + ", "; // COLOR_ID
  sql += to_string(wins) + ", ";     // WINS
  sql += to_string(losses);          // LOSSES
  sql += ");";

  W.exec(sql);

  W.commit();
}

void add_state(connection *C, string name)
{
  pqxx::work W(*C);

  // Assuming the file contains only the NAME column for each STATE
  string sql = "INSERT INTO STATE (NAME) VALUES (";
  sql += W.quote(name);
  sql += ");";

  W.exec(sql);

  W.commit();
}

void add_color(connection *C, string name)
{
  pqxx::work W(*C);

  string sql = "INSERT INTO COLOR (NAME) VALUES (";
  sql += W.quote(name);
  sql += ");";

  W.exec(sql);

  W.commit();
}

// Helper function to append conditions to the SQL query
void appendCondition(string &sql, bool &isFirstCondition, bool useStat, const string &field, string minValue, string maxValue)
{
  if (useStat)
  {
    if (!isFirstCondition)
    {
      sql += " AND ";
    }
    else
    {
      sql += "WHERE ";
      isFirstCondition = false;
    }
    sql += field + " BETWEEN " + (minValue) + " AND " + (maxValue);
  }
}

void printResult(pqxx::result result)
{
  // Print column names
  if (!result.empty())
  {
    for (int colnum = 0; colnum < result.columns(); ++colnum)
    {
      if (colnum > 0)
        cout << " ";
      cout << result.column_name(colnum);
    }
    cout << endl;

    // Iterate over rows
    for (pqxx::result::const_iterator it = result.begin(); it != result.end(); ++it)
    {
      // Iterate over fields in each row
      for (int colnum = 0; colnum < result.columns(); ++colnum)
      {
        if (colnum > 0)
        {
          cout << " ";
        }
        // Print field value; adjust according to the expected data type or use as<string>() for string representation
        cout << it->at(colnum).c_str();
      }
      cout << endl;
    }
  }
  else
  {
    cout << "No result found!" << endl;
  }
}

/*
 * All use_ params are used as flags for corresponding attributes
 * a 1 for a use_ param means this attribute is enabled (i.e. a WHERE clause is needed)
 * a 0 for a use_ param means this attribute is disabled
 */
void query1(connection *C,
            int use_mpg, int min_mpg, int max_mpg,
            int use_ppg, int min_ppg, int max_ppg,
            int use_rpg, int min_rpg, int max_rpg,
            int use_apg, int min_apg, int max_apg,
            int use_spg, double min_spg, double max_spg,
            int use_bpg, double min_bpg, double max_bpg)
{

  string sql = "SELECT * FROM player ";
  bool isFirstCondition = true;
  pqxx::work W(*C);

  // Use the helper function for each statistic
  appendCondition(sql, isFirstCondition, use_mpg == 1, "mpg", to_string(min_mpg), to_string(max_mpg));
  appendCondition(sql, isFirstCondition, use_ppg == 1, "ppg", to_string(min_ppg), to_string(max_ppg));
  appendCondition(sql, isFirstCondition, use_rpg == 1, "rpg", to_string(min_rpg), to_string(max_rpg));
  appendCondition(sql, isFirstCondition, use_apg == 1, "apg", to_string(min_apg), to_string(max_apg));
  appendCondition(sql, isFirstCondition, use_spg == 1, "spg", to_string(min_spg), to_string(max_spg));
  appendCondition(sql, isFirstCondition, use_bpg == 1, "bpg", to_string(min_bpg), to_string(max_bpg));
  pqxx::result R = W.exec(sql);

  // Assuming R is not empty and you want to print column names and their corresponding values
  printResult(R);
}

void query2(connection *C, string team_color)
{
  // select t.name from team t,color c where t.color_id=c.color_id and c.name='DarkBlue';
  pqxx::work W(*C);

  // Construct the SQL query using the team_color parameter
  string sql = "SELECT t.name FROM team t, color c WHERE t.color_id=c.color_id AND c.name=" + W.quote(team_color) + ";";

  // Execute the query
  pqxx::result R = W.exec(sql);
  printResult(R);
}

void query3(connection *C, string team_name)
{
  // select p.first_name, p.last_name, p.ppg from player p, team t  where p.team_id=t.team_id and t.name='UNC' order by p.ppg desc;
  pqxx::work W(*C);
  string sql = "SELECT p.first_name, p.last_name FROM player p, team t WHERE p.team_id=t.team_id AND t.name=" + W.quote(team_name) + "ORDER BY p.ppg DESC;";
  pqxx::result R = W.exec(sql);
  printResult(R);
}

void query4(connection *C, string team_state, string team_color)
{
  // select p.uniform_num, p.first_name, p.last_name from player p,state s,color c, team t where t.state_id=s.state_id and t.color_id=c.color_id and p.team_id = t.team_id and s.name= 'NC' and c.name='DarkBlue';
  pqxx::work W(*C);
  string sql = "SELECT p.uniform_num, p.first_name, p.last_name FROM player p, team t, state s, color c WHERE t.state_id=s.state_id AND t.color_id=c.color_id AND p.team_id = t.team_id AND s.name=" + W.quote(team_state) + "AND c.name=" + W.quote(team_color) + ";";
  pqxx::result R = W.exec(sql);
  printResult(R);
}

void query5(connection *C, int num_wins)
{
  //   SELECT p.first_name, p.last_name, t.name, t.wins FROM player p, team t WHERE p.team_id=t.team_id AND t.wins>10;
  pqxx::work W(*C);
  string sql = "SELECT p.first_name, p.last_name, t.name, t.wins FROM player p, team t WHERE p.team_id=t.team_id AND t.wins>" + to_string(num_wins) + ";";
  pqxx::result R = W.exec(sql);
  printResult(R);
}
