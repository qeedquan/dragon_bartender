#include <allegro5/allegro.h>
#include <allegro5/allegro_native_dialog.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_primitives.h>

#include <math.h>
#include <memory>
#include <string>
#include <list>
#include <vector>
#include <fstream>
#include <iostream>

#define GAME_WIDTH  800
#define GAME_HEIGHT 600

using std::unique_ptr;
using std::string;
using std::list;
using std::vector;
using std::ofstream;
using std::ifstream;
using std::cerr;

ALLEGRO_DISPLAY *d;
ALLEGRO_EVENT_QUEUE *q;
ALLEGRO_TIMER *t;
ALLEGRO_FONT *font;
ALLEGRO_FONT *bigFont;

ALLEGRO_KEYBOARD_STATE keyState;

ALLEGRO_BITMAP *background;
ALLEGRO_BITMAP *bartop;
ALLEGRO_BITMAP *dragon;
ALLEGRO_BITMAP *dragonIcon;
ALLEGRO_BITMAP *cactus;
ALLEGRO_BITMAP *smallCactus;
ALLEGRO_BITMAP *tinyCactus;
ALLEGRO_BITMAP *customer;
ALLEGRO_BITMAP *bandit;
ALLEGRO_BITMAP *instructions;
ALLEGRO_BITMAP *shotglass;
ALLEGRO_BITMAP *fireball;
ALLEGRO_BITMAP *title;

enum class gameState
{
    MAINMENU,
    INSTRUCTIONS,
    INGAME,
    PAUSE,
    GAMEOVER,
    EXIT
};

#define NUMPRIMES 169
int primes[NUMPRIMES] = {2,    3,    5,    7,   11,   13,   17,   19,   23,   29,
                        31,   37,   41,   43,   47,   53,   59,   61,   67,   71,
                        73,   79,   83,   89,   97,  101,  103,  107,  109,  113,
                       127,  131,  137,  139,  149,  151,  157,  163,  167,  173,
                       179,  181,  191,  193,  197,  199,  211,  223,  227,  229,
                       233,  239,  241,  251,  257,  263,  269,  271,  277,  281,
                       283,  293,  307,  311,  313,  317,  331,  337,  347,  349,
                       353,  359,  367,  373,  379,  383,  389,  397,  401,  409,
                       419,  421,  431,  433,  439,  443,  449,  457,  461,  463,
                       467,  479,  487,  491,  499,  503,  509,  521,  523,  541,
                       547,  557,  563,  569,  571,  577,  587,  593,  599,  601,
                       607,  613,  617,  619,  631,  641,  643,  647,  653,  659,
                       661,  673,  677,  683,  691,  701,  709,  719,  727,  733,
                       739,  743,  751,  757,  761,  769,  773,  787,  797,  809,
                       811,  821,  823,  827,  829,  839,  853,  857,  859,  863,
                       877,  881,  883,  887,  907,  911,  919,  929,  937,  941,
                       947,  953,  967,  971,  977,  983,  991,  997, 1009};

int menuSelection = 0;

int score = 0;
int multiplier = 1;
unsigned int totalGuysSeen = 0;
unsigned int hitsInARow = 0;
unsigned int untilNextMultiplier = primes[3];
unsigned int primeCount = 3;

int lives = 3;

const int dragonX = 535;
int dragonBarIndex = 0;
const unsigned int numBars = 4;

const int cactusY = 10;
const int smallCactusY = 0;
const int tinyCactusY = -5;

const float cactusSpeed = -5;
const float smallCactusSpeed = -3;
const float tinyCactusSpeed = -1.5;
float cactusX = 0;
float smallCactusX = 0;
float tinyCactusX = 0;

bool saved = false;
bool saveError = false;

bool loadError = false;

enum class ProjectileType
{
    SHOTGLASS = 0,
    FIREBALL
};

class Projectile
{
public:
    static constexpr float speed = -6.0;
    static constexpr float fireSpeed = -10.0;
    
    ProjectileType type;
    float x;
    int barIndex;

    Projectile (ProjectileType t,
                int bIndex) :
        type (t),
        x (dragonX),
        barIndex (bIndex)
    {}
    
    void move()
    {
        if (type == ProjectileType::FIREBALL)
            x += fireSpeed;
        else
            x += speed;
    }
    
    void draw() const
    {
        int y;
        
        switch (type)
        {
            case ProjectileType::SHOTGLASS:
                y = 110 + (barIndex * 120);
                al_draw_bitmap (shotglass, x, y, 0);
                break;
            case ProjectileType::FIREBALL:
                y = 80 + (barIndex * 120);
                al_draw_bitmap (fireball, x, y, 0);
                break;
        }
    }
};

list <Projectile> projectiles;

enum class GuyType
{
    CUSTOMER = 0,
    BANDIT
};

class Guy
{
public:
    static constexpr float speed = 2.5;
    static constexpr float sinIncrement = ALLEGRO_PI / 20.0;
    static constexpr float stepHeight = 5.0;
    
    GuyType type;
    float x;
    int barIndex;
    float sinOffset;

    Guy() :
        type (((rand() % 4) == 0) ? GuyType::BANDIT : GuyType::CUSTOMER),
        x (-50),
        barIndex (rand() % 4),
        sinOffset (ALLEGRO_PI * (float(rand()) / float(RAND_MAX)))
    {}
    
    bool isCustomer() const
    {
        return type == GuyType::CUSTOMER;
    }
    
    void walk()
    {
        x += speed;
        sinOffset += sinIncrement;
        
        while (sinOffset > ALLEGRO_PI)
            sinOffset -= ALLEGRO_PI;
    }
    
    void draw() const
    {
        const int yStart = 50;
        const int yIncrement = 120;
        
        const int y = yStart + (yIncrement * barIndex) + (stepHeight * sin(sinOffset));
        
        switch (type)
        {
            case GuyType::BANDIT:
                al_draw_bitmap (bandit, x, y, 0);
                break;
            case GuyType::CUSTOMER:
                al_draw_bitmap (customer, x, y, 0);
                break;
        }
    }
};

list <Guy> guys;

void resetGame()
{
    guys.clear();
    projectiles.clear();
    
    score = 0;
    multiplier = 1;
    totalGuysSeen = 0;
    hitsInARow = 0;
    untilNextMultiplier = primes[3];
    primeCount = 3;

    lives = 3;

    dragonBarIndex = 0;
    
    cactusX = 0;
    smallCactusX = 0;
    tinyCactusX = 0;
}

void al_error (const string &errMsg)
{
    al_show_native_message_box (NULL,
                                "Error",
                                "Unable to initialize game",
                                errMsg.c_str(),
                                NULL,
                                ALLEGRO_MESSAGEBOX_ERROR);
    
    exit(1);
}

void updateScore (bool goodShot)
{
    if (goodShot)
    {
        score += multiplier;
                                                
        hitsInARow++;
        if (hitsInARow >= untilNextMultiplier)
        {
            multiplier++;
            primeCount += 2;
            if (primeCount > NUMPRIMES)
                primeCount = primes[NUMPRIMES - 1];
            
            hitsInARow = 0;
            untilNextMultiplier = primes[primeCount];
        }
    }
    else
    {
        multiplier = 1;
        hitsInARow = 0;
        primeCount = 3;
        untilNextMultiplier = primes[primeCount];
    }
}

void saveGame()
{
    ofstream out ("savedgame");
    
    if (!out.good())
    {
        saveError = true;
        return;
    }
    
    out << score << "\n"
        << multiplier << "\n"
        << totalGuysSeen << "\n"
        << hitsInARow << "\n"
        << untilNextMultiplier << "\n"
        << primeCount << "\n"
        << lives << "\n"
        << dragonBarIndex << "\n"
        << cactusX << "\n"
        << smallCactusX << "\n"
        << tinyCactusX << "\n"
        << guys.size() << "\n";
    
    for (const Guy &guy : guys)
    {
        out << int(guy.type) << " "
            << guy.x << " "
            << guy.barIndex << " "
            << guy.sinOffset << "\n";
    }
    
    for (const Projectile &p : projectiles)
    {
        out << int(p.type) << " "
            << p.x << " "
            << p.barIndex << "\n";
    }
    
    out.close();
    saved = true;
}

bool loadGame()
{
    ifstream in ("savedgame");
    
    if (!in.good())
    {
        cerr << "savedgame doesn't exist\n";
        return false;
    }
    
    in >> score;
    if (score < 0)
    {
        cerr << "score < 0\n";
        return false;
    }
    
    in >> multiplier;
    if (multiplier <= 0)
    {
        cerr << "multiplier <= 0\n";
        return false;
    }
    
    in >> totalGuysSeen;
    in >> hitsInARow;
    in >> untilNextMultiplier;
    
    in >> primeCount;
    if (primeCount >= NUMPRIMES)
    {
        cerr << "prime count >= number of stored primes\n";
        return false;
    }
    
    in >> lives;
    if (in <= 0)
    {
        cerr << "lives <= 0\n";
        return false;
    }
    
    in >> dragonBarIndex;
    if (dragonBarIndex > 3)
    {
        cerr << "bad dragon bar index (" << dragonBarIndex << ")\n";
        return false;
    }
    
    in >> cactusX;
    in >> smallCactusX;
    in >> tinyCactusX;
    
    int numGuys;
    in >> numGuys;
    
    if (numGuys < 0 || numGuys > 50)
    {
        cerr << "bad numGuys value\n";
        return false;
    }
    
    for (int i = 0; i < numGuys; i++)
    {
        Guy g;
        
        int t;
        
        in >> t;
        if (t == int(GuyType::BANDIT))
            g.type = GuyType::BANDIT;
        else if (t == int(GuyType::CUSTOMER))
            g.type = GuyType::CUSTOMER;
        else
        {
            cerr << "Bad guy type\n";
            return false;
        }
        
        in >> g.x;
        if (g.x < -100 || g.x > GAME_WIDTH)
        {
            cerr << "Bad guy position\n";
            return false;
        }
        
        in >> g.barIndex;
        if (g.barIndex > 3)
        {
            cerr << "Bad guy bar index\n";
            return false;
        }
        
        in >> g.sinOffset;
        
        guys.push_back (g);
    }
    
    int numProjectiles;
    in >> numProjectiles;
    
    if (numProjectiles < 0 || numProjectiles > 100)
    {
        cerr << "Bad number of projectiles\n";
        return false;
    }
    
    for (int i = 0; i < numProjectiles; i++)
    {
        int t;
        ProjectileType type;
        
        float x;
        int bIndex;
        
        in >> t;
        if (t == int(ProjectileType::SHOTGLASS))
            type = ProjectileType::SHOTGLASS;
        else if (t == int(ProjectileType::FIREBALL))
            type = ProjectileType::FIREBALL;
        else
        {
            cerr << "Bad projectile type\n";
            return false;
        }
        
        in >> x;
        if (x < -100 || x > GAME_WIDTH)
        {
            cerr << "Bad projectile position\n";
            return false;
        }
        
        in >> bIndex;
        if (bIndex > 3)
        {
            cerr << "Bad projectile bar index\n";
            return false;
        }
        
        Projectile p (type, bIndex);
        p.x = x;
        
        projectiles.push_back (p);
    }
    
    return true;
}

int main ()
{
    srand (time (NULL));
    
    if (!al_init())
        al_error ("Error initializing Allegro");
    
    if (!al_install_keyboard())
        al_error ("Error setting up keyboard");
    
    int numAdapters = al_get_num_video_adapters();
    if (numAdapters <= 0)
        al_error ("No video adapters found, somehow...");
    
    ALLEGRO_MONITOR_INFO info;
    if (!al_get_monitor_info (0, &info))
        al_error ("Unable to get monitor info");
    
    const int midx = (info.x2 - info.x1) / 2;
    const int midy = (info.y2 - info.y1) / 2;
    
    al_set_new_display_flags (ALLEGRO_WINDOWED | ALLEGRO_OPENGL);
    al_set_new_window_position (midx - (GAME_WIDTH / 2), midy - (GAME_HEIGHT / 2));
    d = al_create_display (GAME_WIDTH, GAME_HEIGHT);
    
    if (d == NULL)
        al_error ("Unable to create a display");
    
    al_set_window_title (d, "Old West Railroad Dragon Bartender!");
    
    t = al_create_timer (1.0 / 60.0);
    if (t == NULL)
        al_error ("Unable to initialize timing");
    
    q = al_create_event_queue();
    al_register_event_source (q, al_get_display_event_source (d));
    al_register_event_source (q, al_get_keyboard_event_source());
    al_register_event_source (q, al_get_timer_event_source (t));
    
    al_init_font_addon();
    if (!al_init_ttf_addon())
        al_error ("Unable to initialize TTF loader");
    
    font = al_load_font ("resources/FreeMono.ttf", 16, 0);
    bigFont = al_load_font ("resources/FreeMono.ttf", 42, 0);
    
    if (font == NULL || bigFont == NULL)
        al_error ("Unable to load font 'resources/FreeMono.ttf'");
    
    if (!al_init_image_addon())
        al_error ("Unable to initialize image addon");
    
    if (!al_init_primitives_addon())
        al_error ("Unable to initialize primitives addon");
    
    background = al_load_bitmap ("resources/background.png");
    if (background == NULL)
        al_error ("Unable to load 'resources/background.png'");
    
    bartop = al_load_bitmap ("resources/bartop.png");
    if (bartop == NULL)
        al_error ("Unable to load 'resources/bartop.png'");
    
    dragon = al_load_bitmap ("resources/dragon.png");
    if (dragon == NULL)
        al_error ("Unable to load 'resources/dragon.png'");
    
    dragonIcon = al_load_bitmap ("resources/dragonicon.png");
    if (dragonIcon == NULL)
        al_error ("Unable to load 'resources/dragonicon.png'");
    
    cactus = al_load_bitmap ("resources/cactus1.png");
    if (cactus == NULL)
        al_error ("Unable to load 'resources/cactus1.png'");
    
    smallCactus = al_load_bitmap ("resources/cactus2.png");
    if (smallCactus == NULL)
        al_error ("Unable to load 'resources/cactus2.png'");
    
    tinyCactus = al_load_bitmap ("resources/cactus3.png");
    if (tinyCactus == NULL)
        al_error ("Unable to load 'resources/cactus3.png'");
    
    customer = al_load_bitmap ("resources/customer.png");
    if (customer == NULL)
        al_error ("Unable to load 'resources/customer.png'");
    
    bandit = al_load_bitmap ("resources/bandit.png");
    if (bandit == NULL)
        al_error ("Unable to load 'resources/bandit.png'");
    
    instructions = al_load_bitmap ("resources/instructions.png");
    if (instructions == NULL)
        al_error ("Unable to load 'resources/instructions.png'");
    
    shotglass = al_load_bitmap ("resources/shotglass.png");
    if (shotglass == NULL)
        al_error ("Unable to load 'resources/shotglass.png'");
    
    fireball = al_load_bitmap ("resources/fireball.png");
    if (fireball == NULL)
        al_error ("Unable to load 'resources/fireball.png'");
    
    title = al_load_bitmap ("resources/title.png");
    if (title == NULL)
        al_error ("Unable to load 'resources/title.png'");
    
    const ALLEGRO_COLOR sand = al_map_rgb (255, 193, 20);
    const ALLEGRO_COLOR sky = al_map_rgb (110, 255, 255);
    const ALLEGRO_COLOR magicPink = al_map_rgb (255, 0, 255);
    const ALLEGRO_COLOR black = al_map_rgb (0, 0, 0);
    const ALLEGRO_COLOR white = al_map_rgb (255, 255, 255);
    const ALLEGRO_COLOR red = al_map_rgb (255, 64, 64);
    
    const ALLEGRO_COLOR darken = al_map_rgba (0, 0, 0, 92);
    
    al_convert_mask_to_alpha (background, magicPink);
    al_convert_mask_to_alpha (bartop, magicPink);
    al_convert_mask_to_alpha (dragon, magicPink);
    al_convert_mask_to_alpha (dragonIcon, magicPink);
    al_convert_mask_to_alpha (cactus, magicPink);
    al_convert_mask_to_alpha (smallCactus, magicPink);
    al_convert_mask_to_alpha (tinyCactus, magicPink);
    al_convert_mask_to_alpha (customer, magicPink);
    al_convert_mask_to_alpha (bandit, magicPink);
    al_convert_mask_to_alpha (shotglass, magicPink);
    al_convert_mask_to_alpha (fireball, magicPink);
    al_convert_mask_to_alpha (title, magicPink);
    
    al_set_display_icon (d, dragonIcon);
    
    al_start_timer (t);
    
    gameState state = gameState::MAINMENU;
    bool draw = true;
    
    while (state != gameState::EXIT)
    {
        ALLEGRO_EVENT event;
        
        if (draw && al_is_event_queue_empty(q))
        {
            if (state == gameState::INGAME ||
                state == gameState::PAUSE)
            {
                al_clear_to_color (sand);
                al_draw_filled_rectangle (0, 0, GAME_WIDTH, 7, sky);
                
                al_draw_bitmap (tinyCactus, int(tinyCactusX), tinyCactusY, 0);
                al_draw_bitmap (smallCactus, int(smallCactusX), smallCactusY, 0);
                al_draw_bitmap (cactus, int(cactusX), cactusY, 0);
                
                al_draw_bitmap (background, 0, 0, 0);
                
                al_draw_bitmap (dragonIcon, GAME_WIDTH - 230, 0, 0);
                al_draw_textf (bigFont, black, GAME_WIDTH - 130, 0, 0, "x %d", lives);
                
                const int barStartY = 110;
                const int barSpaceY = 120;
                for (unsigned int i = 0; i < numBars; i++)
                {
                    for (const Guy &guy : guys)
                    {
                        if ((unsigned int)guy.barIndex == i)
                            guy.draw();
                    }
                    
                    al_draw_bitmap (bartop, 0, barStartY + (barSpaceY * i), 0);
                    
                    for (const Projectile &p : projectiles)
                    {
                        if ((unsigned int)p.barIndex == i)
                            p.draw();
                    }
                }
                
                const int dragonBarYOffset = -50;
                al_draw_bitmap (dragon, dragonX, dragonBarYOffset + barStartY + (barSpaceY * dragonBarIndex), 0);
                
                al_draw_textf (font, black, 0, 0, 0, "Score: %d     Multiplier: x%d  (%d until next)", score, multiplier, untilNextMultiplier - hitsInARow);
                
                if (state == gameState::PAUSE)
                {
                    const int centerY = GAME_HEIGHT / 2 - 1 - al_get_font_line_height(bigFont);
                    const int bottomY = GAME_HEIGHT - 50 - al_get_font_line_height(bigFont);
                    
                    al_draw_filled_rectangle (0, 0, GAME_WIDTH, GAME_HEIGHT, darken);
                    al_draw_text (bigFont,
                                  white,
                                  GAME_WIDTH / 2,
                                  centerY,
                                  ALLEGRO_ALIGN_CENTRE,
                                  "PAUSED");
                    
                    if (saveError)
                    {
                        al_draw_text (bigFont,
                                      red,
                                      GAME_WIDTH / 2,
                                      bottomY,
                                      ALLEGRO_ALIGN_CENTRE,
                                      "Error saving game");
                    }
                    else if (saved)
                    {
                        al_draw_text (bigFont,
                                      white,
                                      GAME_WIDTH / 2,
                                      bottomY,
                                      ALLEGRO_ALIGN_CENTRE,
                                      "Game saved");
                    }
                    else
                    {
                        al_draw_text (bigFont,
                                      white,
                                      GAME_WIDTH / 2,
                                      bottomY,
                                      ALLEGRO_ALIGN_CENTRE,
                                      "Press S to save the game");
                    }
                }
            }
            else if (state == gameState::MAINMENU)
            {
                al_clear_to_color (sand);
                
                al_draw_bitmap (title, 222, 100, 0);
                
                al_draw_text (bigFont,
                              black,
                              GAME_WIDTH / 2 - 100,
                              GAME_HEIGHT - 220,
                              0,
                              "Start");
                
                if (loadError)
                {
                    al_draw_text (bigFont,
                                  red,
                                  GAME_WIDTH / 2 - 100,
                                  GAME_HEIGHT - 170,
                                  0,
                                  "Error");
                }
                else
                {
                    al_draw_text (bigFont,
                                  black,
                                  GAME_WIDTH / 2 - 100,
                                  GAME_HEIGHT - 170,
                                  0,
                                  "Continue");
                }
                
                al_draw_text (bigFont,
                              black,
                              GAME_WIDTH / 2 - 100,
                              GAME_HEIGHT - 120,
                              0,
                              "Instructions");
                
                al_draw_bitmap (dragonIcon,
                                GAME_WIDTH / 2 - 200,
                                GAME_HEIGHT - 220 + (50 * menuSelection),
                                0);
            }
            else if (state == gameState::INSTRUCTIONS)
            {
                al_draw_bitmap (instructions, 0, 0, 0);
            }
            else if (state == gameState::GAMEOVER)
            {
                al_clear_to_color (sand);
                al_draw_filled_rectangle (0, 0, GAME_WIDTH, 7, sky);
                
                al_draw_bitmap (tinyCactus, int(tinyCactusX), tinyCactusY, 0);
                al_draw_bitmap (smallCactus, int(smallCactusX), smallCactusY, 0);
                al_draw_bitmap (cactus, int(cactusX), cactusY, 0);
                
                al_draw_bitmap (background, 0, 0, 0);
                
                al_draw_bitmap (dragonIcon, GAME_WIDTH - 230, 0, 0);
                al_draw_textf (bigFont, black, GAME_WIDTH - 130, 0, 0, "x %d", lives);
                
                const int barStartY = 110;
                const int barSpaceY = 120;
                for (unsigned int i = 0; i < numBars; i++)
                {
                    for (const Guy &guy : guys)
                    {
                        if ((unsigned int)guy.barIndex == i)
                            guy.draw();
                    }
                    
                    al_draw_bitmap (bartop, 0, barStartY + (barSpaceY * i), 0);
                    
                    for (const Projectile &p : projectiles)
                    {
                        if ((unsigned int)p.barIndex == i)
                            p.draw();
                    }
                }
                
                const int centerY = GAME_HEIGHT / 2 - 1 - al_get_font_line_height(bigFont);
                
                al_draw_filled_rectangle (0, 0, GAME_WIDTH, GAME_HEIGHT, darken);
                al_draw_text (bigFont,
                              white,
                              GAME_WIDTH / 2,
                              centerY,
                              ALLEGRO_ALIGN_CENTRE,
                              "GAME OVER");
                
                al_draw_textf (bigFont,
                               white,
                               GAME_WIDTH / 2,
                               centerY + 50,
                               ALLEGRO_ALIGN_CENTRE,
                               "Your final score was %d",
                               score);
            }
            
            al_flip_display();
            draw = false;
        }
        
        al_wait_for_event (q, &event);
        
        switch (event.type)
        {
            case ALLEGRO_EVENT_DISPLAY_CLOSE:
                state = gameState::EXIT;
                break;
            case ALLEGRO_EVENT_DISPLAY_SWITCH_OUT:
                if (state == gameState::INGAME)
                    state = gameState::PAUSE;
                break;
            case ALLEGRO_EVENT_KEY_DOWN:
            {
                if (event.keyboard.keycode == ALLEGRO_KEY_SPACE ||
                    event.keyboard.keycode == ALLEGRO_KEY_P)
                {
                    if (state == gameState::PAUSE)
                    {
                        state = gameState::INGAME;
                    }
                    else if (state == gameState::INGAME)
                    {
                        state = gameState::PAUSE;
                        saved = false;
                        saveError = false;
                    }
                    else if (state == gameState::INSTRUCTIONS)
                    {
                        state = gameState::MAINMENU;
                    }
                }
                else if (event.keyboard.keycode == ALLEGRO_KEY_ENTER)
                {
                    if (state == gameState::MAINMENU)
                    {
                        if (menuSelection == 1)
                        {
                            resetGame();
                            
                            bool loadOK = loadGame();
                            loadError = !loadOK;
                            
                            if (loadOK)
                                state = gameState::PAUSE;
                        }
                        else if (menuSelection == 2)
                        {
                            state = gameState::INSTRUCTIONS;
                        }
                        else
                        {
                            resetGame();
                            state = gameState::INGAME;
                        }
                    }
                    else if (state == gameState::INSTRUCTIONS)
                    {
                        state = gameState::MAINMENU;
                        loadError = false;
                    }
                    else if (state == gameState::GAMEOVER)
                    {
                        state = gameState::MAINMENU;
                        loadError = false;
                        
                        resetGame();
                    }
                }
                else if (event.keyboard.keycode == ALLEGRO_KEY_ESCAPE)
                {
                    state = gameState::EXIT;
                }
                else if (event.keyboard.keycode == ALLEGRO_KEY_UP)
                {
                    if (state == gameState::INGAME &&
                        dragonBarIndex > 0)
                    {
                        dragonBarIndex--;
                    }
                    else if (state == gameState::MAINMENU)
                    {
                        menuSelection--;
                        if (menuSelection < 0)
                            menuSelection = 2;
                    }
                }
                else if (event.keyboard.keycode == ALLEGRO_KEY_DOWN)
                {
                    if (state == gameState::INGAME &&
                        (unsigned int)dragonBarIndex < numBars - 1)
                    {
                        dragonBarIndex++;
                    }
                    else if (state == gameState::MAINMENU)
                    {
                        menuSelection++;
                        if (menuSelection > 2)
                            menuSelection = 0;
                    }
                }
                else if (event.keyboard.keycode == ALLEGRO_KEY_Z)
                {
                    if (state == gameState::INGAME)
                        projectiles.push_back (Projectile (ProjectileType::SHOTGLASS, dragonBarIndex));
                }
                else if (event.keyboard.keycode == ALLEGRO_KEY_X)
                {
                    if (state == gameState::INGAME)
                        projectiles.push_back (Projectile (ProjectileType::FIREBALL, dragonBarIndex));
                }
                else if (event.keyboard.keycode == ALLEGRO_KEY_S)
                {
                    if (state == gameState::PAUSE && !saved)
                    {
                        saveGame();
                        saved = true;
                    }
                }
                break;
            }
            case ALLEGRO_EVENT_TIMER:
            {
                switch (state)
                {
                    case gameState::PAUSE:
                        break;
                    case gameState::INGAME:
                        {
                            tinyCactusX += tinyCactusSpeed;
                            smallCactusX += smallCactusSpeed;
                            cactusX += cactusSpeed;
                            
                            if (tinyCactusX + 35 < 0)
                                tinyCactusX = GAME_WIDTH;
                            if (smallCactusX + 70 < 0)
                                smallCactusX = GAME_WIDTH;
                            if (cactusX + 140 < 0)
                                cactusX = GAME_WIDTH;
                            
                            static int spawner = 0;
                            if (spawner <= 0)
                            {
                                guys.push_back (Guy());
                                totalGuysSeen++;
                                
                                spawner = 100;
                                
                                unsigned int i = 0;
                                int primeIterator = 0;
                                while (i < totalGuysSeen)
                                {
                                    spawner -= 5;
                                    i += primes[primeIterator];
                                    primeIterator++;
                                }
                                
                                if (spawner < 5)
                                    spawner = 5;
                            }
                            else
                            {
                                spawner--;
                            }
                            
                            for (auto guy = guys.begin(); guy != guys.end();)
                            {
                                guy->walk();
                                
                                if (guy->x > 545)
                                {
                                    guy = guys.erase (guy);
                                    lives--;
                                    
                                    if (lives <= 0)
                                        state = gameState::GAMEOVER;
                                }
                                else
                                    guy++;
                            }
                            
                            for (auto p = projectiles.begin(); p != projectiles.end();)
                            {
                                p->move();
                                
                                bool hit = false;
                                for (auto guy = guys.begin(); guy != guys.end();)
                                {
                                    if (p->barIndex == guy->barIndex &&
                                        p->x > guy->x &&
                                        p->x < guy->x + 60)
                                    {
                                        bool goodShot = false;
                                        
                                        if (guy->isCustomer())
                                        {
                                            if (p->type == ProjectileType::SHOTGLASS)
                                                goodShot = true;
                                        }
                                        else
                                        {
                                            if (p->type == ProjectileType::FIREBALL)
                                                goodShot = true;
                                        }
                                        
                                        updateScore (goodShot);
                                        
                                        p = projectiles.erase (p);
                                        guys.erase (guy);
                                        
                                        hit = true;
                                        break;
                                    }
                                    
                                    guy++;
                                }
                                
                                if (hit)  // this projectile hit a guy, and no longer exists
                                    continue;
                                
                                if (p->x < -10)
                                    p = projectiles.erase (p);
                                else
                                    p++;
                            }
                        }
                        break;
                    default:
                        break;
                }
                
                draw = true;
                break;
            }
        }
    }
    
    
    al_destroy_bitmap (background);
    al_destroy_bitmap (bartop);
    al_destroy_bitmap (dragon);
    al_destroy_bitmap (dragonIcon);
    al_destroy_bitmap (cactus);
    al_destroy_bitmap (smallCactus);
    al_destroy_bitmap (tinyCactus);
    al_destroy_bitmap (customer);
    al_destroy_bitmap (bandit);
    al_destroy_bitmap (instructions);
    al_destroy_bitmap (shotglass);
    al_destroy_bitmap (fireball);
    al_destroy_bitmap (title);
    
    return 0;
}

