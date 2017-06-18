Here is a function I typed up for Dwarf Therapist .  It is the labor optimizer functionality that is in the latest version of Dwarf Therapist.  Coded in C++.  The function can be found in mainwindow.cpp

The 3 files I worked on are dfinstance.cpp, dwarf.cpp, and mainwindow.cpp.  The majority of the function is in initializeSuperStruct()
void MainWindow::initializeSuperStruct()
{
using namespace std;

QList<Dwarf*> m_selected_dwarfs = m_view_manager->get_selected_dwarfs();
int d_count = m_selected_dwarfs.size();

//get labors per dwarf
bool ok;
QString labors_p = QInputDialog::getText(0, tr(“Labors per dwarf”),
tr(“How many dwarves do you want to assign per labor?”), QLineEdit::Normal,””, &ok);
if (!ok)
return;
int labors_per = labors_p.toInt();

//used to derive %’s for dwarfs
float totalCoverage = 0;
//Miner, Hunter, Woodcutter total Coverage;
float MHWCoverage = 0;
float modifiedTotalCoverage = 0;

//get csv filename
ok = 0;

QString file_name = QInputDialog::getText(0, tr(“CSV input file”),
tr(“Name of input file for assigning labors?”), QLineEdit::Normal,””, &ok);
if (!ok)
return;
//int file_name = csv_n.toInt();

ok = 0;

QString percent_jobs = QInputDialog::getText(0, tr(“Percent of Population/Jobs to assign”),
tr(“What percent do you wish to assign? (include decimal, i.e. .7)”), QLineEdit::Normal,””, &ok);
if (!ok)
return;
float pct_jobs = percent_jobs.toFloat();

//dwarf count, to be pulled from selection
//labor count, to be pulled from init .csv
int role_count = 0;
int total_jobs = m_selected_dwarfs.size() * labors_per;
//rounding is done later
float jobs_to_assign = total_jobs * pct_jobs;

bool MHWExceed = 0;

string st;

//SUPERSTRUCT!
QVector <superStruct> sorter;

QVector <roles> ListOfRolesV;

//ifstream f( filename.c_str() );
//ifstream f(“role_labor_list.csv”);
string f_holder = file_name.toStdString();
ifstream f(f_holder.c_str());
//ifstream f (file_name.c_str());

//loops through each line in the file
while (getline (f, st))
{
int pos = 1;
//placeholder
roles p_holder;

//gets the line for processing
istringstream iss( st );

//loops through each , in one line
while (getline (iss, st, ‘,’))
{
//role_name, labor_id, priority_weight, num_to_assign
if (pos == 1)
{
p_holder.role_name = QString::fromStdString(st);
}
else if (pos == 2)
{
int fieldValue = 0;
istringstream (st) >> fieldValue;

p_holder.labor_id = fieldValue;
}
else if (pos == 3)
{
float fieldValue = 0.0f;
istringstream (st) >> fieldValue;

p_holder.priority = fieldValue;
}
else if (pos == 4)
{
float fieldValue = 0;
istringstream (st) >> fieldValue;

p_holder.coverage = fieldValue;
}
//initialize numberAssigned to 0;
p_holder.numberAssigned = 0;
pos++;
}
ListOfRolesV.push_back(p_holder);
pos = 1;
}

f.close();

role_count = ListOfRolesV.size();

//calculate total Coverage
for (int x = 0; x < role_count; x++)
{
int labor = ListOfRolesV[x].labor_id;
if ((labor == 0) || (labor == 10) || (labor == 44))
{
MHWCoverage = MHWCoverage + ListOfRolesV[x].coverage;

}
totalCoverage += ListOfRolesV[x].coverage;
}

//Check if ((MHWCoverage / totalCoverage ) * total_jobs )> m_selected_dwarfs.size();
if (((MHWCoverage / totalCoverage) * jobs_to_assign ) > m_selected_dwarfs.size())
{
if (DT->user_settings()->value(“options/labor_exclusions”,true).toBool())
{
//only set flag if option is checked in options
MHWExceed = 1;
}

modifiedTotalCoverage = totalCoverage – MHWCoverage;
}
else
{
modifiedTotalCoverage = totalCoverage;
}

//time to set numberToAssign
for (int x = 0; x < role_count; x++)
{
int labor = ListOfRolesV[x].labor_id;
float pre_number_to_assign = 0;

if ( (MHWExceed) && ((labor == 0) || (labor == 10) || (labor == 44)))
{
//truncate miner, hunter, woodcutter to population size
pre_number_to_assign = ListOfRolesV[x].coverage / MHWCoverage * m_selected_dwarfs.size();
}
else if (MHWExceed)
{
//since miner, hunter, woodcutter are being truncated down to population size, we need to remove that from the jobs to assign
pre_number_to_assign = ListOfRolesV[x].coverage / modifiedTotalCoverage * (jobs_to_assign – m_selected_dwarfs.size());
}
else
{
//normal
pre_number_to_assign = (ListOfRolesV[x].coverage / modifiedTotalCoverage) * jobs_to_assign;
}
//old, works without conflicting labors
//float pre_number_to_assign = (ListOfRolesV[x].coverage / totalCoverage) * jobs_to_assign;

//rounding magic
if (( pre_number_to_assign + 0.5) >= (int(pre_number_to_assign ) + 1) )
{
ListOfRolesV[x].numberToAssign = int(pre_number_to_assign)+1;
}
else
{
ListOfRolesV[x].numberToAssign = int(pre_number_to_assign);
}
}

//now time to initialize the superStruct

//count # dwarf’s
//foreach(Dwarf *d, m_view_manager->get_selected_dwarfs()) {d_count++;}

//resize based on # dwarfs * # of roles
sorter.resize(d_count*role_count);
QString *temp;

int sstruct_pos = 0;
//for(int sstruct_pos = 0; sstruct_pos < sorter.size(); sstruct_pos++)
{
//cycle through each Dwarf,
foreach (Dwarf *d, m_view_manager->get_selected_dwarfs())
{
int dwarf_count = 0;
//cycle through each Role in ListOfRolesV
for (int role_entry = 0; role_entry < ListOfRolesV.size(); role_entry++)
{
//problem here… need a role per dwarf which I have, but Dwarf doesn’t have anywhere to save a w_percent…
//Could just load into superStruct, but it won’t be a pointer…
sorter[sstruct_pos].d_id = d->id();
sorter[sstruct_pos].labor_id = ListOfRolesV[role_entry].p_l_id();
//QString role_name = ListOfRolesV[role_entry].p_r_name();
//not sure if this is what I want
sorter[sstruct_pos].role_name = ListOfRolesV[role_entry].p_r_name();

//can’t use pointers with this
sorter[sstruct_pos].r_percent = d->get_raw_role_rating(ListOfRolesV[role_entry].role_name);
sorter[sstruct_pos].w_percent = ListOfRolesV[role_entry].priority * d->get_raw_role_rating(ListOfRolesV[role_entry].role_name);

sstruct_pos++;
}
//probably not needed
dwarf_count ++;

}
}
//used for debug pause
int test = 0;

//sort superstruct by w_percent

//runs through list
for (int i = 0; i < sorter.size(); i++)
{
//checks against lower element
//cout << *s->at(i).pPercent << endl;
for (int j = 1; j < (sorter.size()-i); j++)
{
//using pointer dereferences messed this up, but, not using them doesn’t sort right
if (sorter[j-1].w_percent < sorter[j].w_percent)
{
swap(sorter[j-1], sorter[j]);
}
}
}

//now to assign labors!

//vs running through each labor, and assigning up to max # dwarf’s…
//1. need to start from top of allRoles
//2. see what labor is, see if goal is met.
//DONE (via csv input) for DEPLOY APP, need to change this to see if labor is asked for
//3. Y – Skip (use while statement)
//4. N –
//a. Check Dwarf to see if he is available (again, while Statement),
//not available if has conflicting labor
//  Y – Assign
//  N – Skip

//start from top of list.
for (int sPos = 0; sPos < sorter.size(); sPos++)
{

//dwarf position (used for searching of name)

//I would like to set these to pointers, but I was getting out of bound issues…
int dPos = 0;
//role position
int rPos = 0;
//cycle through each dwarf? I should, but, I can use the *s->at(x).name to find the Dwarf #
//no, need to compare roleName to dwarfName

//search for Dwarf (y) for that role

for (int d = 0; d < d_count; d++)
{
//dPos is never getting updated.
//int temp = d->id();
if (sorter[sPos].d_id == m_selected_dwarfs.at(d)->id())
{
dPos = d;
//break;
};
//dPos++;
}

//match superStruct roleName to myRoles name position (to check if filled up)
for (int r = 0; r < ListOfRolesV.size(); r++)
{
if (sorter[sPos].role_name == ListOfRolesV[r].role_name)
{
rPos = r;
//break;
};
}

//search if role @ rPos is filled up
//role check started…
int number_to_assign = *ListOfRolesV[rPos].n_assign();

if (ListOfRolesV[rPos].numberAssigned < number_to_assign)
{
int labor = ListOfRolesV[rPos].labor_id;
bool incompatible = 0;
//Miner is 0
//Woodcutter is 10
//Hunter is 44
//only set incompatible flag if option is set.
if (DT->user_settings()->value(“options/labor_exclusions”,true).toBool())
{
if ((labor == 0) || (labor == 10) || (labor == 44))
{
if ((m_selected_dwarfs.at(dPos)->labor_enabled(0)) || (m_selected_dwarfs.at(dPos)->labor_enabled(10)) || (m_selected_dwarfs.at(dPos)->labor_enabled(44)))
{
//dwarf has an incompatible labor, set flag
incompatible = 1;
}
}
}

//need to compare to total_assigned_labors,
//this will be used for Custom Professions

//I’m going to have to have a dwarf labors_assigned variable to count for
//CUSTOM PROFESSIONS
int d_currentAssigned = m_selected_dwarfs.at(dPos)->get_dirty_labors().size();
//m_selected_dwarfs.at(dPos)->total_assigned_labors();

//is Dwarf (d) laborsAssigned full? Used in proto-type
if (d_currentAssigned < labors_per)
{
//if Dwarf doesn’t have an incompatible labor enabled, proceed.
if (incompatible == 0)
{
//another check to check for conflicting laborid’s,
//AND
//to see if labor_id flag is checked in Options.
//m_selected_dwarfs.at(dPos)->toggle_labor(ListOfRolesV[rPos].labor_id);
m_selected_dwarfs.at(dPos)->toggle_labor(labor);
//assign labor (z) to Dwarf @ dPos
//add labor (z?) to Dwarf’s (d) labor vector

//m_selected_dwarfs.at(dPos).laborsAssigned.push_back(r->at(rPos).name);

//increase labors assigned to roles vector
//role check completed
ListOfRolesV[rPos].numberAssigned++;
}
}
incompatible = 0;
}

}
m_model->calculate_pending();

};

BlackJack Project
/* ========================================================================== */
/* */
/* BlackJack.cpp */
/* (c) 2008 Joshua Laferriere */
/* */
/* Description */
/* This code compiles with Dev-C++ and it produces a neato chart of hi-lo */
/* values. It also creates a bjchart.csv file useable to create charts. */
/* ========================================================================== */

#include
#include
#include
#include
#include
#include
#include
#include

using namespace std;

ofstream bjCharts; //used to print cards

//used for creating a deck of cards.
struct cards
{
int index;
string faceValue;
string suite;

//static variable

//static int shoePosition;

//static void incrementCardCounter()
//{
// cardCounter++;
//}

//default constructor

cards::cards()
{
index = 0;
//int cards::cardCounter=0;
};

//int cards::cardCounter=0;
};

//used for individual cards of player/dealer’s hand.
//duh, this should extend the above class.
struct card
{
string faceValue;
string suite;
int value;
//index of card used for hand, this value will be set from 1-4 within the hands struct.
int index;

void getValue()
{
if(faceValue==”A”)
{
value=11;
}
if(faceValue==”2″){value=2;}
if(faceValue==”3″){value=3;}
if(faceValue==”4″){value=4;}
if(faceValue==”5″){value=5;}
if(faceValue==”6″){value=6;}
if(faceValue==”7″){value=7;}
if(faceValue==”8″){value=8;}
if(faceValue==”9″){value=9;}
if(faceValue==”10″){value=10;}
if(faceValue==”J”){value=10;}
if(faceValue==”Q”){value=10;}
if(faceValue==”K”){value=10;}
}

//default constructor
card()
{
index = 0;
faceValue = “”;
suite = “”;
value = 0;
};

};

//going to be used for later implementation.
struct shoe
{
cards cArds[52];
int shoePosition;
};

//used for creating players hand
struct hands
{
card cArd[5]; //5 cards (0-4)
int index; //# of hand.
int sumTotal;
int numCards;

//sets a single ace to 1
void setAceTo1 ()
{
for(int setAceTo1Counter=0; setAceTo1Counter<=numCards; setAceTo1Counter++)
{
if(cArd[setAceTo1Counter].value==11)
{
cArd[setAceTo1Counter].value=1;
break;
}
}
}

//return true if ace (11) is found
bool aceW11()
{
for(int aceW11Counter=0; aceW11Counter<=numCards; aceW11Counter++)
{
if(cArd[aceW11Counter].value==11)
{return true;}
}
return false;
}

//returns number of Aces, used in aceCheck

//not used yet
int getSum()
{
//I included this and it didn’t work
//…
//aceCheck();
sumTotal=0;

for (int z=0; z<=numCards; z++)
{
sumTotal = (cArd[z].value + sumTotal);
}

return sumTotal;
}

//default constructor for hands
hands::hands()
{
index = 0; //# of hand.
sumTotal = 0;
numCards = 0;
}

};

void initShoe(cards shoe[], int numDecks)
{
int i=0; // index of shoe
string suite;

for(int b = 0; b<numDecks; b++) // 0 to numdecks
{
for(int s=0;s<4;s++) // 0 to 3 suites
{
if(s==0){suite=”Clubs”;}
if(s==1){suite=”Hearts”;}
if(s==2){suite=”Spades”;}
if(s==3){suite=”Diamonds”;}

for(int a=0;a<13;a++) // 0-12 number of cards per suite
{
shoe[i].index=i;

//cout << “/n” << “(s)suite: ” << suite;
shoe[i].suite=suite;

//cout << “\n” << shoe[i].suite;

if(a==0){shoe[i].faceValue=”A”;}
if(a==1){shoe[i].faceValue=”2″;}
if(a==2){shoe[i].faceValue=”3″;}
if(a==3){shoe[i].faceValue=”4″;}
if(a==4){shoe[i].faceValue=”5″;}
if(a==5){shoe[i].faceValue=”6″;}
if(a==6){shoe[i].faceValue=”7″;}
if(a==7){shoe[i].faceValue=”8″;}
if(a==8){shoe[i].faceValue=”9″;}
if(a==9){shoe[i].faceValue=”10″;}
if(a==10){shoe[i].faceValue=”J”;}
if(a==11){shoe[i].faceValue=”Q”;}
if(a==12){shoe[i].faceValue=”K”;}
//cout << “\n” << “(b)Deck: ” << b << ” ” << “(s)Suite: ” << suite << ” ” << “(a)Card: ” << a;

//cout << “\n” << shoe[i].faceValue;
//cout << “\n” << shoe[i].suite << ” “;
//cout << “\n”;

i++;
}
}
}
}

void printShoe(const cards shoe[], int sizeOfShoe) //I was thinking of passing this by reference, but in C++, array’s are passed by reference automatically. There is no local copy made.
{
int value = 0;
int addedValue = 0;
int lowestValue=0;
int highestValue=0;

bjCharts.open(“bjChart.csv”, fstream::app); //append

//cout << “\nSize of Shoe: ” << sizeOfShoe;

//this below code is to output a chart for viewing purposes

for(int i=0; i<sizeOfShoe; i++)
{
if( (shoe[i].faceValue==”A”) || (shoe[i].faceValue==”10″) || (shoe[i].faceValue==”J”) || (shoe[i].faceValue==”Q”) || (shoe[i].faceValue==”K”) )
{
addedValue = (-1);
}
if( (shoe[i].faceValue==”2″) || (shoe[i].faceValue==”3″) || (shoe[i].faceValue==”4″) || (shoe[i].faceValue==”5″) || (shoe[i].faceValue==”6″) )
{
addedValue = 1;
}
if( (shoe[i].faceValue==”7″) || (shoe[i].faceValue==”8″) || (shoe[i].faceValue==”9″) )
{
addedValue = 0;
}

//card count value
value = value + addedValue;
//cout << “\nCard Count: ” << value << “/n”;

bjCharts << i << “,” << value << “/n”;

if(value<0)
{
for(int x = 0; x < ( ( ( ( (sizeOfShoe/52) *4) +6) ) + value); x++) //precede with blanks
{
cout << ” “;
}
for(int x=0; x<(value*(-1));x++) //fill with x
{
cout << “x”;
}
cout << “0”;
cout << “\n”;
}
else
if(value>0) // if value is greater than 0
{
for(int x=0; x<( ( (sizeOfShoe/52) *4) +6); x++)
{
cout << ” “;
}
cout << “0”;
for(int x=0; x<value; x++)
{
cout << “x”;
}
cout << “\n”;
}
else
if(value==0)
{
for(int x=0; x<( ( (sizeOfShoe/52) *4) +6); x++)
{
cout << ” “;
}
cout << “0”;
cout << “\n”;
}

//keep track of lowest and highest values
/*
if(value<lowestValue)
{
system(“pause”);
cout << “\nLowest Value:” << value << “\n”;
lowestValue=value;
}

if(value>highestValue)
{
system(“pause”);
cout << “\nHighest Value:” << value << “\n”;
highestValue=value;
}
*/

}
bjCharts.close();
}

void shuffleShoe(cards shoe[], int sizeOfShoe)
{
//— Shuffle elements by randomly exchanging each with one other.
for (int i=0; i<(sizeOfShoe-1); i++) //i is each card incrementally
{
int r = i + (rand() % (sizeOfShoe-i)); // Random remaining position.

cards temp = shoe[i]; //temp card is value of i
shoe[i] = shoe[r]; //move i to random position
shoe[r] = temp; //old random position is temp value
//it does this every card. In affect, the deck is being shuffled twice
}
}

//this function should return a value that is the player’s hand.
//this way this value can be stored.
//need to keep track of shoe[] place.
//unless I do dealer’s hand within dealCards

//a similar function should be used to return the dealer’s hand.
//this way the value can be stored.
//then the values will be compared.

//maybe dealcards can call two other functions
//one is dealer
//one is player
//have to pass a, that’s it. A is my counter.

//returns dealer’s value
int dealer(cards shoe[], int sizeOfShoe, int card_Counter)
{

hands dealer[4];

//used to keep track of cards.
int x = 0;

//keep hitting until 17 is met.

for (int y = card_Counter+1; y<sizeOfShoe; y++)
{
//ensure dealer hasn’t busted.
while( (dealer[0].getSum() > 21) && (dealer[0].aceW11()) )
{
dealer[0].setAceTo1();
}
if (dealer[0].getSum() > 16){break;}

dealer[0].cArd[x].faceValue=shoe[y].faceValue;
dealer[0].cArd[x].index=x;
dealer[0].cArd[x].suite=shoe[y].suite;
dealer[0].numCards=x;
dealer[0].cArd[x].getValue();

cout << “\nCard: ” << dealer[0].cArd[x].faceValue;

cout << “\nCard value: ” << dealer[0].cArd[x].value;

cout << “\nDealer Sum: ” << dealer[0].getSum() << “\n”;

x++;

//system(“pause”);

}

cout << “\nDealer Sum: ” << dealer[0].getSum() << “\n”;
return dealer[0].getSum();

}

void dealCards(cards shoe[], int sizeOfShoe)
{
//note: can’t use hand[0], confuses it with the hand[4] that’s defined within hands struct

//having problem using vectors. Doesn’t find members of hands struct within vector
//vector ha[1];

//used for hand, up to four hands (0-3)
hands player[4];

char hit;

//x is a counter for the card in the hand.
int x=0;

int shoePosition = 0;

//sumtotal is a static var
//ha[0].sumTotal=0;

/*
deal cards until sizeOfshoe has been met
*/

//go through deck
for (int a=0; a<sizeOfShoe; a++)
{
// verify Sum isn’t over 21

if (player[0].getSum()>21){cout << “\n” << “You Busted!” << “\n”; break;}

if (player[0].numCards == 4){cout << “5 cards dealt, you win.” << “\n”; break;}

player[0].cArd[x].faceValue=shoe[a].faceValue;
player[0].cArd[x].index=x;
player[0].cArd[x].suite=shoe[a].suite;
player[0].numCards=x;

player[0].cArd[x].getValue();

cout << “\nCard: ” << player[0].cArd[x].faceValue;

//ha[0].sumTotal = ha[0].sumTotal + ha[0].cArd[x].value;

cout << “\nCard value: ” << player[0].cArd[x].value;

//should this be inside the getSum() after the 21 check?

while( (player[0].getSum() > 21) && (player[0].aceW11()) )
{
player[0].setAceTo1();
}

cout << “\nTotal: ” << player[0].getSum();

x++;

shoePosition = a;

//cout << “\nshoe p: ” << shoePosition << ” ” << a << “\n”;
cout << “\n(s)tand\n”;
cin >> hit;

if (hit==’s’) break;

}
//return player[0].getSum();

int dealerValue = dealer(shoe, sizeOfShoe, shoePosition);

if ( (player[0].getSum()>21) && dealerValue>21) {cout << “\nYou both busted!\n”;}
else if (player[0].getSum()>21) {cout << “\nYou busted! Dealer wins!\n”;}
else if (dealerValue>21) {cout << “\nDealer busted! You win!\n”;}
else if (dealerValue>=player[0].getSum()) {cout << “\nDealer wins!\n”;}
else {cout << “\nYou win!\n”;}

//if (dealerValue>=player[0].getSum()){cout << “\nDealer wins.”;}
//else {cout << “\nPlayer wins!”;};

//system(“pause”);
}

//for dealer
/*
{
hands dealer[1];
int d =0;
while (dealer[0].getSum() >16)
{
dealer[0].cArd[x].faceValue=shoe[a].faceValue;
dealer[0].cArd[x].index=x;
dealer[0].cArd[x].suite=shoe[a].suite;
dealer[0].numCards=x;

dealer[0].cArd[x].getValue();

cout << “\nCard: ” << dealer[0].cArd[x].faceValue;

//ha[0].sumTotal = ha[0].sumTotal + ha[0].cArd[x].value;

cout << “\nCard value: ” << player[0].cArd[x].value;

while( (dealer[0].getSum() > 16) && (dealer[0].aceW11()) )
{
dealer[0].setAceTo1();
}

}

}
*/

int main()
{
int player;
int dealer;

int *cardCounter;

cardCounter=new int;

*cardCounter=0;

system(“del bjChart.csv”);
/* initialize random seed: */
srand ( time(NULL) );

int numDecks;

cout << “# of decks? \n”;
cin >> numDecks;
cout << “\n”;

int sizeOfShoe = numDecks*52;

//initialize shoe
cards shoe[numDecks*52];

//static int shoePosition=0;

//set values
initShoe(shoe, numDecks);

//print before shuffle
//printShoe(shoe, sizeOfShoe);

//print shoe
int numRuns = 1;

cout << “\nNumber of runs?”;
cin >> numRuns;

for(int r=0;r<numRuns;r++)
{
shuffleShoe(shoe, sizeOfShoe);
//print after shuffle
printShoe(shoe, sizeOfShoe);
}

/*dealer = */
dealCards(shoe, sizeOfShoe);
system(“pause”);

main();

}

 

MultiThreaded Coin Toss App
#include
#include
#include #include
#include
#include
#include
#include
//#include //doesn't work for comma's
//#include <sys/resource.h>

//stack linker setting
//-Wl,–stack,167772160

//future goal, implement a rng x rng new number.

#define PHI 0x9e3779b9

static uint32_t Q[4096], c = 362436;

using namespace std;

const int range = 4000000;
const int halfRange = range / 2;

//max array size is 4 million
//const long arraySize = 4000000;
const long arraySize = 10000000;
const int numThreads = 10;

std::uniform_int_distribution<> two(1,2);
std::uniform_int_distribution<> three(-1,1);
std::uniform_int_distribution<> onehk(1,range);

void main2(int);

class cmwc
{
public:

void init_rand(uint32_t x)
{
int i;

Q[0] = x;
Q[1] = x + PHI;
Q[2] = x + PHI + PHI;

for (i = 3; i < 4096; i++)
Q[i] = Q[i – 3] ^ Q[i – 2] ^ PHI ^ i;
}

uint32_t rand_cmwc(void)
{
uint64_t t, a = 18782LL;
static uint32_t i = 4095;
uint32_t x, r = 0xfffffffe;
i = (i + 1) & 4095;
t = a * Q[i] + c;
c = (t >> 32);
x = t + c;
if (x < c) {
x++;
c++;
}
return (Q[i] = r – x);
}

};

struct myThreadStructure
{
int *value;

//int array[arraySize];

long runningTotal = 0;

typedef std::mt19937 MyRNG;

MyRNG rng;
cmwc rng_1;

int returnValue()
{
return *value;
}

void initSeed(int number)
{
//standard
rng.seed(((unsigned int)time(NULL))+number);
//cmwc
rng_1.init_rand(time(NULL)+number);
}

void initArray()
{
//checks value for engine type
if (*value == 0)
{
for (int x = 0; x < arraySize; x++)
{
//I moved the if statement outside…
//array[x] = (*value == 0) ? (array[x] = ((rng_1.rand_cmwc()%range + 1) <= halfRange ? 1: -1)) : (array[x] = ((onehk(rng)) <= halfRange ? 1: -1));

//I get rid of the array here.
//array[x] = ((rng_1.rand_cmwc()%range + 1) <= halfRange ? 1: -1);
//runningTotal += array[x];
runningTotal += ((rng_1.rand_cmwc()%range + 1) <= halfRange ? 1: -1);
}
}
else if (*value == 1)
{
for (int x = 0; x < arraySize; x++)
{
//array[x] = ((onehk(rng)) <= halfRange ? 1: -1);
//runningTotal += array[x];
runningTotal += ((onehk(rng)) <= halfRange ? 1: -1);
}
}

}
//cout << runningTotal << endl;

};

//Thank You! -> passing arguments to thread
//http://www.tutorialspoint.com/cplusplus/cpp_multithreading.htm

unsigned __stdcall myThread(void *data)
{
//works?
//int *x = static_cast<int*>(data);

struct myThreadStructure *my_data;

//good for single elements, not for multiple
//myThreadStructure *x = static_cast<myThreadStructure*>(data);

my_data = (struct myThreadStructure *) data;

my_data->initArray();

_endthreadex(0);
}

int main()
{
//doesn’t work for comma’s
//std::cout.imbue(std::locale(“”));

int numRuns;

int long1 = arraySize*numThreads;

cout << “Each run does ” << long1 << endl;

cout << “Minimum of 10 runs, or divide by 0 error occurs.” << endl;
cout << “Number of Runs: “;
cin >> numRuns;

main2(numRuns);

//creates indefinant loop
//while(true);

return 0;
}

void main2(int numberOfRuns)
{

ofstream myfile;
myfile.open (“coinToss.csv”);

HANDLE hThread[numThreads];

myThreadStructure myThreadData[10];

//useable for passing a value to a pointer within a structure
//int x = number;
//myThreadData[number].value = &x;

long total = 0;

int percentage;

if (numberOfRuns > 9) {percentage = (numberOfRuns / 10);}

//these only need to be done once
{
for (int num = 0; num < numThreads; num++)
{
//give one half of threads one engine
int x = (num < (numThreads/2) ) ? 0: 1;
myThreadData[num].value = &x;
//if (num > (numThreads/2) ) myThreadData[num].value = 1;
cout << myThreadData[num].returnValue();
}
//have to initialize seeds separately.
}

for (int x = 0; x < numberOfRuns; x++)
{

//re init array (good to ensure randomness)
for (int number = 0; number < numThreads; number++)
{
myThreadData[number].initSeed(number);
}

//cout << percentage << endl;

for (int number = 0; number < numThreads; number++)
{
//parameters: ?, ?, entry function, arglist, stacksize (0 for OS to determine), ?
hThread[number] = (HANDLE)_beginthreadex(NULL, 0, myThread, (void*)&myThreadData[number], 0, NULL);
}

//need to set the first parameter to an actual # if I want it to wait, 0 doesn’t wait, or set it to an array name
WaitForMultipleObjects(numThreads, hThread, TRUE, INFINITE);

CloseHandle(hThread);

for (int p = 0; p < numThreads; p++)
{
//cout << myThreadData[p].runningTotal << endl;
total = total + myThreadData[p].runningTotal;
//cout << myThreadData[p].runningTotal << endl;
myThreadData[p].runningTotal = 0;
//cout << myThreadData[p].runningTotal << endl;
}
myfile << /*(long)(arraySize*x*numThreads) << “,” << */ total << endl;

//delete [] myThreadData;
if ((x % percentage) == 0) cout << endl << x;

}
cout << “Total: ” << total << endl;

}