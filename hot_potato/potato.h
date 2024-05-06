#include <vector>
#include <string>
#include <sstream>


#define BACKLOG 100   

class Potato{
public:
    int hops_left;   // Number of hops left before game ends
    //std::vector<int> potato_trace;    
    int potato_trace[512];
    int origin_hops;

    int i = 0;
    
    // Default constructor
    Potato() : hops_left(0), origin_hops(0) {};
    // Constructor
    Potato(int num_hops): hops_left(num_hops), origin_hops(num_hops)
    {
         memset(this->potato_trace, 0, sizeof(this->potato_trace));
    };

    // Game ends
    bool isGameEnds() const{
        return hops_left <= 0;
    }

    // Record the trace of playerID and decrease hops_left
    void recordPassTo(int playerID){
            //potato_trace.push_back(playerID); 
            potato_trace[i] = playerID;
            i = i + 1;
            hops_left = hops_left - 1;
    }
    // Print potato trace
    void printTrace(){

        std::cout << "Trace of potato:" << std::endl;
        for(size_t i = 0; i < origin_hops; ++i){
            std::cout << potato_trace[i];
            if(i < origin_hops - 1){
                std::cout << ",";
            }
        }
        std::cout << std::endl;
    }


};