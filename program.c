#include <ncurses.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <getopt.h>

#define MAX_FISH 80
#define HIGH_SCORE_FILE "fisherman_highscore.txt"

typedef struct {
    int x, y;
    int dir;
    int type;
    int subtype;
    int speed;
    int alive;
} Fish;

static Fish fish[MAX_FISH];
static int fish_count = 0;
static int max_x, max_y;
static int boat_x;
static int hook_x = -1, hook_y = -1;
static int score = 0;
static int level = 1;
static int tick = 0;
static unsigned int rng_state = 0;
static int difficulty = 1;
static useconds_t tick_delay = 120000;
static int fish_spawn_target = 6;

const char *fish_small_r[] = { ">><>", ">=>", "><>" };
const char *fish_small_l[] = { "<><<", "<=<", "<><" };

const char *fish_med_r[]   = { ">>==(((°>", ">><(((>", ">>--((°>" };
const char *fish_med_l[]   = { "<°)))==<<", "<(((><<", "<°))--<<" };

const char *fish_big_r[]   = { ">>>{{{((°>>>", ">>=={{{(((>>>" } ;
const char *fish_big_l[]   = { "<<<°))))}}<<<", "<<<(((}}}==<<" } ;

int rnd(int n) {
    if (n <= 0) return 0;
    if (rng_state) {
        rng_state = rng_state * 1103515245u + 12345u;
        return (rng_state >> 16) % n;
    }
    return rand() % n;
}

void load_highscore(int *hs) {
    FILE *f = fopen(HIGH_SCORE_FILE, "r");
    if (!f) { *hs = 0; return; }
    if (fscanf(f, "%d", hs) != 1) *hs = 0;
    fclose(f);
}
void save_highscore(int hs) {
    FILE *f = fopen(HIGH_SCORE_FILE, "w");
    if (!f) return;
    fprintf(f, "%d\n", hs);
    fclose(f);
}

void init_colors() {
    if (!has_colors()) return;
    start_color();
    use_default_colors();
    init_pair(1, COLOR_WHITE, -1);
    init_pair(2, COLOR_YELLOW, -1);
    init_pair(3, COLOR_CYAN, -1);
    init_pair(4, COLOR_MAGENTA, -1);
    init_pair(5, COLOR_GREEN, -1);
    init_pair(6, COLOR_RED, -1);
    init_pair(7, COLOR_BLUE, -1);
}

void spawn_fish(int n, int sea_top) {
    for (int i = 0; i < n && fish_count < MAX_FISH; i++) {
        Fish *f = &fish[fish_count];
        f->alive = 1;

        int r = rnd(100);
        if (r < 55) f->type = 0;
        else if (r < 85) f->type = 1;
        else f->type = 2;

        f->subtype = rnd(3);

        f->dir = rnd(2) ? 1 : -1;

        f->y = sea_top + 3 + rnd(max_y - sea_top - 7);
        if (f->y > max_y - 4) f->y = max_y - 4;

        if (f->type == 0) f->speed = 1;
        else if (f->type == 1) f->speed = 2;
        else f->speed = 1 + rnd(2);

        f->x = (f->dir == 1) ? 1 : max_x - 20;

        fish_count++;
    }
}

void draw_waves(int sea_top) {
    for (int x = 0; x < max_x; x++) {
        int offset = (x + tick / 5) % 6;
        int y = sea_top + (offset < 3 ? 0 : 1);
        mvaddch(y, x, offset < 3 ? '~' : ',');
    }
}

void draw_boat() {
    int bx = boat_x - 6;
    if (bx < 1) bx = 1;
    if (bx + 12 > max_x) bx = max_x - 12;

    attron(COLOR_PAIR(2) | A_BOLD);
    mvprintw(3, bx,   "   __/\\__   ");
    mvprintw(4, bx,   "  /______\\  ");
    mvprintw(5, bx+2, "<(^)  (^)>");
    attroff(COLOR_PAIR(2) | A_BOLD);
}

void draw_hook() {
    if (hook_y < 0) return;

    attron(COLOR_PAIR(5));
    for (int y = 6; y <= hook_y && y < max_y - 2; y++)
        mvaddch(y, hook_x, '|');
    mvaddch(hook_y, hook_x, 'J');
    attroff(COLOR_PAIR(5));
}

void draw_fish() {
    for (int i = 0; i < fish_count; i++) {
        if (!fish[i].alive) continue;

        const char *art;
        int color;

        if (fish[i].type == 0) {
            art = (fish[i].dir == 1)
                    ? fish_small_r[ fish[i].subtype % 3 ]
                    : fish_small_l[ fish[i].subtype % 3 ];
            color = 3;
        } else if (fish[i].type == 1) {
            art = (fish[i].dir == 1)
                    ? fish_med_r[ fish[i].subtype % 3 ]
                    : fish_med_l[ fish[i].subtype % 3 ];
            color = 4;
        } else {
            art = (fish[i].dir == 1)
                    ? fish_big_r[ fish[i].subtype % 2 ]
                    : fish_big_l[ fish[i].subtype % 2 ];
            color = 7;
        }

        int fx = fish[i].x;
        int len = strlen(art);

        if (fx < 1) fx = 1;
        if (fx + len >= max_x) fx = max_x - len - 1;

        attron(COLOR_PAIR(color));
        mvprintw(fish[i].y, fx, "%s", art);
        attroff(COLOR_PAIR(color));
    }
}

void advance_fish() {
    for (int i = 0; i < fish_count; i++) {
        if (!fish[i].alive) continue;

        int sp = fish[i].speed > 0 ? fish[i].speed : 1;
        if (tick % (4 - sp) != 0) continue;

        fish[i].x += fish[i].dir * sp;

        if (fish[i].x < 1) {
            fish[i].x = 1;
            fish[i].dir = 1;
        }
        if (fish[i].x > max_x - 20) {
            fish[i].x = max_x - 20;
            fish[i].dir = -1;
        }
    }
}

void update_hook(int sea_top) {
    if (hook_y < 0) return;

    hook_y++;

    if (hook_y >= max_y - 2) {
        hook_y = -1;
        return;
    }

    for (int i = 0; i < fish_count; i++) {
        if (!fish[i].alive) continue;

        const char *art;
        if (fish[i].type == 0)
            art = (fish[i].dir == 1) ? fish_small_r[fish[i].subtype % 3] : fish_small_l[fish[i].subtype % 3];
        else if (fish[i].type == 1)
            art = (fish[i].dir == 1) ? fish_med_r[fish[i].subtype % 3] : fish_med_l[fish[i].subtype % 3];
        else
            art = (fish[i].dir == 1) ? fish_big_r[fish[i].subtype % 2] : fish_big_l[fish[i].subtype % 2];

        int len = strlen(art);

        if (hook_y == fish[i].y && hook_x >= fish[i].x && hook_x < fish[i].x + len) {
            fish[i].alive = 0;
            score += (fish[i].type == 0 ? 1 : fish[i].type == 1 ? 3 : 7);
            hook_y = -1;
            return;
        }
    }
}

void maintain_fish(int sea_top) {
    if (score >= 200) return;

    int alive = 0;
    for (int i = 0; i < fish_count; i++)
        if (fish[i].alive) alive++;

    if (alive < fish_spawn_target)
        spawn_fish(fish_spawn_target - alive + 1, sea_top);
}

void check_level_up() {
    static int last = 0;
    int need = 5 + (level - 1) * 3;

    if (score - last >= need) {
        level++;
        last = score;
        fish_spawn_target++;
    }
}

void draw_hud() {
    attron(COLOR_PAIR(6) | A_BOLD);
    mvprintw(0, 0, "+");
    for (int i = 1; i < max_x - 1; i++) mvaddch(0, i, '-');
    mvaddch(0, max_x - 1, '+');

    mvprintw(1, 1, "LEVEL:%d  SCORE:%d  HIGH:%d  DIFF:%s",
             level, score,
             0,
             difficulty == 0 ? "Easy" : difficulty == 1 ? "Med" : "Hard");

    mvprintw(2, 0, "+");
    for (int i = 1; i < max_x - 1; i++) mvaddch(2, i, '-');
    mvaddch(2, max_x - 1, '+');
    attroff(COLOR_PAIR(6) | A_BOLD);
}

void show_menu() {
    clear();
    mvprintw(max_y/2 - 3, max_x/2 - 12, "Fisherman - Choose difficulty: ");
    mvprintw(max_y/2,     max_x/2 - 6,  "1 - Easy");
    mvprintw(max_y/2 + 1, max_x/2 - 6,  "2 - Medium");
    mvprintw(max_y/2 + 2, max_x/2 - 6,  "3 - Hard");
    mvprintw(max_y/2 + 5, max_x/2 - 18, "Press 1/2/3 to choose (ESC pre default)");
    refresh();
}

int main(int argc, char **argv) {
    int opt;
    int start_level = 1;

    while ((opt = getopt(argc, argv, "l:s:")) != -1) {
        if (opt == 'l') start_level = atoi(optarg);
        else if (opt == 's') rng_state = atoi(optarg);
    }

    if (!rng_state) srand(time(NULL));

    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    curs_set(0);

    init_colors();
    getmaxyx(stdscr, max_y, max_x);

    show_menu();
    nodelay(stdscr, FALSE);
    int sel = getch();
    nodelay(stdscr, TRUE);

    if (sel == '1') {
        difficulty = 0;
        tick_delay = 150000;
        fish_spawn_target = 5;
    } else if (sel == '3') {
        difficulty = 2;
        tick_delay = 80000;
        fish_spawn_target = 10;
    } else {
        difficulty = 1;
        tick_delay = 120000;
        fish_spawn_target = 7;
    }

    level = start_level;

    int sea_top = 6;
    fish_count = 0;

    spawn_fish(fish_spawn_target + 3, sea_top);

    boat_x = max_x / 2;

    int highscore = 0;
    load_highscore(&highscore);

    int running = 1;
    int ch;

    while (running) {
        tick++;
        clear();
        getmaxyx(stdscr, max_y, max_x);

        draw_hud();
        draw_boat();
        draw_waves(sea_top);
        draw_fish();
        draw_hook();

        refresh();

        ch = getch();
        switch (ch) {
            case KEY_LEFT:
                if (boat_x > 6) boat_x--;
                break;
            case KEY_RIGHT:
                if (boat_x < max_x - 6) boat_x++;
                break;
            case ' ':
                if (hook_y < 0) {
                    hook_x = boat_x;
                    hook_y = sea_top;
                }
                break;
            case 'q':
            case 'Q':
                running = 0;
                break;
            default: break;
        }

        advance_fish();
        update_hook(sea_top);
        maintain_fish(sea_top);
        check_level_up();

        if (score > highscore) highscore = score;

        if (score >= 200) {
            clear();
            mvprintw(max_y/2, max_x/2 - 10, "Congratulations! You reached 200 points! Score:%d", score);
            mvprintw(max_y/2+2, max_x/2 - 10, "Press q to quit...");
            refresh();
            nodelay(stdscr, FALSE);
            while ((ch = getch()) != 'q' && ch != 'Q');
            running = 0;
        }

        usleep(tick_delay);
    }

    save_highscore(highscore);
    endwin();

    printf("Thanks for playing! Score:%d High:%d\n", score, highscore);
    return 0;
}

