#include <stdio.h>
#include <stdlib.h>
#include <SDL.h>
#include <time.h>

#include <string>
#include <chrono>
#include <thread>

static const int FPS = 35;
//static const int FPS = 2;

enum GameEvent {
    EVENT_END_GAME,
    EVENT_MOVE_UP,
    EVENT_MOVE_DOWN,
    EVENT_MOVE_LEFT,
    EVENT_MOVE_RIGHT,
    EVENT_MOVE_NONE,
    EVENT_BTN_SPECIAL
};

enum FieldType {
    FT_EMPTY,
    FT_ZONK,
    FT_BASE,
    FT_MURPHY,
    FT_INFOTRON,
    FT_CHIP,
    FT_BORDER,
    FT_EXIT,
    FT_ORANGE_DISK,
    FT_PORTAL_EAST,
    FT_PORTAL_SOUTH,
    FT_PORTAL_WEST,
    FT_PORTAL_NORTH,
    FT_PORTAL_EAST_2,
    FT_PORTAL_SOUTH_2,
    FT_PORTAL_WEST_2,
    FT_PORTAL_NORTH_2,
    FT_SNIK_SNAK,
    FT_YELLOW_DISK,
    FT_TERMINAL,
    FT_RED_DISK,
    FT_PORTAL_NS,
    FT_PORTAL_WE,
    FT_PORTAL_CROSS,
    FT_STARS,
    FT_ELECTRON,
    FT_CHIP_WE_1,
    FT_CHIP_WE_2,
    FT_SENSOR,
    FT_BULLET_GREEN,
    FT_BULLET_BLUE,
    FT_BULLET_RED,
    FT_HAZARD,
    FT_RESISTOR,
    FT_CONDENSATOR,
    FT_RESISTORS_NS,
    FT_RESISTORS_WE,
    FT_CHIP_NS_1,
    FT_CHIP_NS_2
};

enum Direction {
    DIR_NONE,
    DIR_UP,
    DIR_DOWN,
    DIR_LEFT,
    DIR_RIGHT
};

const unsigned int HINT_NONE = 0;
const unsigned int HINT_SKIP = 1; /**< Skip field from processing in this round. */
const unsigned int HINT_EXPLOSION = 2; /**< Field is exploding */
const unsigned int HINT_EXPLOSION_INFOTRON = 4; /**< Field is exploding into infotron. */
const unsigned int HINT_EXPLOSION_ORIGIN = 8; /**< Field where the explosion has occured. */
const unsigned int HINT_FALL = 16; /**< Item on field is in process of falling down. */
const unsigned int HINT_FROM_TOP = 32; /**< Object on this field come from top */
const unsigned int HINT_FROM_BOTTOM = 64; /**< Object on this field come from bottom */
const unsigned int HINT_FROM_LEFT = 128; /**< Object on this field come from left */
const unsigned int HINT_FROM_RIGHT = 256; /**< Object on this field come from right */
const unsigned int HINT_LEAVING = 512; /**< Object is leaving the field, it is untouchable for one game step. */
const unsigned int HINT_WAS_BASE = 1024; /**< Field was base before Murphy entered. */
const unsigned int HINT_WAS_INFOTRON = 2048; /**< Field was infotron before Murphy entered. */
const unsigned int HINT_WAS_RED_DISK = 4096; /**< Field was red disk before Murphy entered. */
const unsigned int HINT_TURN_LEFT = 8192; /**< Snik snak rotation to left. */
const unsigned int HINT_TURN_RIGHT = 16384; /**< Snik snak rotation to right. */
const unsigned int HINT_PUSH = 32768; /**< Murphy pushing object */

const int EXPLOSION_STEPS = 3;

struct Point {
    int x;
    int y;

    Point(): x(0), y(0) {}
    Point(int x, int y): x(x), y(y) {}
};

struct Field {
    Point coords;
    FieldType type;
    unsigned int hint;
    int countdown;

    Field(): type(FT_EMPTY), hint(HINT_NONE) {}

    void set_hint(unsigned int h) {
        hint |= h;
    }

    void del_hint(unsigned int h) {
        hint &= ~h;
    }

    bool has_hint(unsigned int h) {
        return (hint & h) > 0;
    }

    /**
     * Return true when this field should be affected by explosion.
     */
    bool affected_by_explosion() {
        switch (type) {
            case FT_EMPTY:
            case FT_ZONK:
            case FT_BASE:
            case FT_MURPHY:
            case FT_INFOTRON:
            case FT_CHIP:
            case FT_EXIT:
            case FT_ORANGE_DISK:
            case FT_SNIK_SNAK:
            case FT_YELLOW_DISK:
            case FT_TERMINAL:
            case FT_RED_DISK:
            case FT_STARS:
            case FT_ELECTRON:
            case FT_CHIP_WE_1:
            case FT_CHIP_WE_2:
            case FT_CHIP_NS_1:
            case FT_CHIP_NS_2:
                return true;

            default:
                return false;
        }
    }

    /**
     * Return true, if this field explodes on impact.
     */
    bool explodes() {
        switch (type) {
            case FT_RED_DISK:
            case FT_ORANGE_DISK:
            case FT_YELLOW_DISK:
            case FT_SNIK_SNAK:
            case FT_STARS:
            case FT_MURPHY:
                return true;

            default:
                return false;
        }
    }

    /**
     * Field type that is set after the explosion.
     */
    FieldType explodes_into() {
        if (type == FT_STARS) {
            return FT_INFOTRON;
        } else {
            return FT_EMPTY;
        }
    }

    bool rolls_on_impact() {
        switch (type) {
            case FT_ZONK:
            case FT_INFOTRON:
            case FT_CHIP:
            case FT_CHIP_NS_1:
            case FT_CHIP_NS_2:
            case FT_CHIP_WE_1:
            case FT_CHIP_WE_2:
                return !has_hint(HINT_FALL);

            default:
                return false;
        }
    }

    std::string to_string() {
        std::string out;

        switch (type) {
            case FT_EMPTY:       out = "EMPTY   "; break;
            case FT_ZONK:        out = "ZONK    "; break;
            case FT_BASE:        out = "BASE    "; break;
            case FT_MURPHY:      out = "MURPHY  "; break;
            case FT_INFOTRON:    out = "INFOTRON"; break;
            case FT_EXIT:        out = "EXIT    "; break;
            case FT_ORANGE_DISK: out = "O_DISK  "; break;
            case FT_SNIK_SNAK:   out = "SNIK_S  "; break;
            case FT_YELLOW_DISK: out = "Y_DISK  "; break;
            case FT_TERMINAL:    out = "TERMINAL"; break;
            case FT_RED_DISK:    out = "R_DISK  "; break;
            case FT_STARS:       out = "STARS   "; break;
            case FT_ELECTRON:    out = "ELECTRON"; break;
            default:             out = "FIXTURE "; break;
        }

        out += " [" + std::to_string(coords.x) + "x" + std::to_string(coords.y) + "] [";

        bool first = false;
        auto hint_fmt = [&](unsigned int hint, const char *desc){
            if (this->hint & hint) {
                if (first) {
                    first = false;
                } else {
                    out += ", ";
                }
                out += desc;
            }
        };

        hint_fmt(HINT_SKIP, "SKIP");
        hint_fmt(HINT_EXPLOSION, "EXPLOSION");
        hint_fmt(HINT_EXPLOSION_INFOTRON, "EXPLOSION_INFOTRON");
        hint_fmt(HINT_EXPLOSION_ORIGIN, "EXPLOSION_ORIGIN");
        hint_fmt(HINT_FALL, "FALL");
        hint_fmt(HINT_FROM_TOP, "FROM_TOP");
        hint_fmt(HINT_FROM_BOTTOM, "FROM_BOTTOM");
        hint_fmt(HINT_FROM_LEFT, "FROM_LEFT");
        hint_fmt(HINT_FROM_RIGHT, "FROM_RIGHT");
        hint_fmt(HINT_LEAVING, "LEAVING");
        hint_fmt(HINT_WAS_BASE, "WAS_BASE");
        hint_fmt(HINT_WAS_INFOTRON, "WAS_INFOTRON");
        hint_fmt(HINT_WAS_RED_DISK, "WAS_RED_DISK");
        hint_fmt(HINT_TURN_LEFT, "TURN_LEFT");
        hint_fmt(HINT_TURN_RIGHT, "TURN_RIGHT");

        out += "]";

        return out;
    }
};

class Level {
public:
    static const int LEVEL_BYTES = 1536;
    static const int LEVEL_WIDTH = 60;
    static const int LEVEL_HEIGHT = 24;
    static const int LEVEL_NAME_LENGTH = 23;

    Field data[LEVEL_WIDTH * LEVEL_HEIGHT];
    bool gravitation, freeze_zonks;
    char title[LEVEL_NAME_LENGTH];

    bool murphy_alive;

public:
    Level(const char *file_name, int level): gravitation(false), freeze_zonks(false), murphy_alive(true) {

#ifndef _WIN32
        FILE *f = fopen(file_name, "rb");
#else
		FILE *f;
		fopen_s(&f, file_name, "rb");
#endif

        if (f) {
            fseek(f, LEVEL_BYTES * (level - 1), SEEK_SET);

            char databytes[LEVEL_WIDTH * LEVEL_HEIGHT];
            fread(databytes, sizeof(char), LEVEL_WIDTH * LEVEL_HEIGHT, f);

            for (size_t i = 0; i < LEVEL_WIDTH * LEVEL_HEIGHT; ++i) {
                data[i].coords = Point(i % LEVEL_WIDTH, i / LEVEL_WIDTH);
                data[i].type = (FieldType)databytes[i];

                if (data[i].type == FT_MURPHY) {
                    murphy = data[i].coords;
                }
            }

            fseek(f, 4, SEEK_CUR); // unused 4B

            char byte;
            fread(&byte, sizeof(char), 1, f);

            gravitation = byte == 1;

            fseek(f, 1, SEEK_CUR); // unused 1B

            fread(title, sizeof(char), LEVEL_NAME_LENGTH, f);

            fread(&byte, sizeof(char), 1, f);
            freeze_zonks = byte == 2;

            // TODO: Gravity switch ports

            fclose(f);
        } else {
            // TODO: Throw error if level file cannot be open.
        }
    }

    bool game_step() {
        //printf("Game step.\n");

        //printf("Murphy at %dx%d.\n", murphy.x, murphy.y);
        //printf("%s\n", data[data_idx(murphy)].to_string().c_str());
        //printf("%s\n", data[data_idx(next_point(murphy, DIR_UP))].to_string().c_str());
        //printf("%s\n", data[data_idx(next_point(murphy, DIR_DOWN))].to_string().c_str());
        //printf("Field 36x13: %s\n", data[data_idx(Point(36, 13))].to_string().c_str());
        //printf("Field 36x14: %s\n", data[data_idx(Point(36, 14))].to_string().c_str());

        if (next_move != DIR_NONE && murphy_alive) {
            Point next = next_point(murphy, next_move);
            Field &fld = data[data_idx(next)];
			Field &murphy_fld = data[data_idx(murphy)];

            bool allow_move = false;

			murphy_fld.del_hint(HINT_PUSH);

            switch (fld.type) {
                case FT_BASE:
                    fld.set_hint(HINT_WAS_BASE);
                case FT_EMPTY:
					// Explode murphy if there is something leaving the field... but only if it is not murphy itself.
                    if (fld.has_hint(HINT_LEAVING) && !(murphy_fld.has_hint(hint_from_direction(turn_back(next_move))))) {
                        explode_9(fld, FT_BASE);
                    } else {
                        allow_move = true;
                    }
                    break;

                case FT_INFOTRON:
                    // TODO: Eat infotron
                    fld.set_hint(HINT_WAS_INFOTRON);
                    allow_move = true;
                    break;

                case FT_RED_DISK:
                    // TODO: Increment disks
                    fld.set_hint(HINT_WAS_RED_DISK);
                    allow_move = true;
                    break;

				case FT_ZONK:
				case FT_ORANGE_DISK:
				case FT_YELLOW_DISK:
					// Crash to falling objects.
					if (fld.has_hint(HINT_FALL)) {
						explode_9(fld, FT_BASE);

					// Allow pushing only to left or right, and yellow disk in any direction.
					} else if (fld.type == FT_YELLOW_DISK || next_move == DIR_LEFT || next_move == DIR_RIGHT) {
						Point more = next_point(next, next_move);
						Field &fld_more = data[data_idx(more)];

						if (fld_more.type == FT_EMPTY && !fld_more.has_hint(HINT_LEAVING)) {
							murphy_fld.set_hint(HINT_PUSH);
							if (murphy_fld.countdown == 1) {
								fld_more.type = fld.type;
								fld_more.set_hint(hint_from_direction(next_move) | HINT_SKIP);
								allow_move = true;
								murphy_fld.countdown = 0;
							}
							else {
								murphy_fld.countdown = 1;
							}
						}
					}
					break;

                default:
                    allow_move = false;
                    break;
            }

			// Reset countdown used for push.
			if (!murphy_fld.has_hint(HINT_PUSH)) {
				murphy_fld.countdown = 0;
			}

            if (allow_move) {
                Field &origin = data[data_idx(murphy)];
                origin.type = FT_EMPTY;
                origin.set_hint(HINT_LEAVING | HINT_SKIP);
                origin.del_hint(HINT_FROM_BOTTOM | HINT_FROM_TOP | HINT_FROM_RIGHT | HINT_FROM_LEFT | HINT_WAS_INFOTRON | HINT_WAS_BASE | HINT_WAS_RED_DISK);

                fld.type = FT_MURPHY;
				fld.set_hint(hint_from_direction(next_move) | HINT_SKIP);

				if (origin.has_hint(HINT_PUSH)) {
					fld.set_hint(HINT_PUSH);
					origin.del_hint(HINT_PUSH);
				}

                murphy = next;
            }
        }
        next_move = DIR_NONE;

		for (int i = 0; i < width() * height(); ++i) {
			Field &field = data[i];
			if (field.has_hint(HINT_SKIP)) {
				continue;
			}

			if (field.has_hint(HINT_LEAVING)) {
				field.del_hint(HINT_LEAVING);
			}
		}

        // Do NPC actions
        for (int i = 0; i < width() * height(); ++i) {
            Field &field = data[i];
            if (field.has_hint(HINT_SKIP)) {
                continue;
            }

            // NPC direction must be determined here.
            Direction dir = DIR_UP;
            if (field.has_hint(HINT_FROM_BOTTOM)) {
                dir = DIR_UP;
            } else if (field.has_hint(HINT_FROM_TOP)) {
                dir = DIR_DOWN;
            } else if (field.has_hint(HINT_FROM_LEFT)) {
                dir = DIR_RIGHT;
            } else if (field.has_hint(HINT_FROM_RIGHT)) {
                dir = DIR_LEFT;
            }

            // Remove hints from Murphy's movement.
            if (field.type != FT_SNIK_SNAK && field.type != FT_STARS) {
                field.del_hint(HINT_FROM_BOTTOM | HINT_FROM_TOP | HINT_FROM_RIGHT | HINT_FROM_LEFT);
            }

            field.del_hint(HINT_WAS_BASE | HINT_WAS_INFOTRON | HINT_WAS_RED_DISK);

            if (field.has_hint(HINT_EXPLOSION) || field.has_hint(HINT_EXPLOSION_INFOTRON)) {
                if (field.countdown > 0) {
                    if (field.countdown == EXPLOSION_STEPS && !field.has_hint(HINT_EXPLOSION_ORIGIN)) {
                        // Test whether we don't need to cascade explode.
                        if (field.explodes()) {
                            explode_9(field, field.explodes_into());
                        }
                    }

                    field.countdown -= 1;
					field.set_hint(HINT_SKIP);
                } else {
                    if (field.has_hint(HINT_EXPLOSION)) {
                        field.type = FT_EMPTY;
                        field.del_hint(HINT_EXPLOSION);
                    } else if (field.has_hint(HINT_EXPLOSION_INFOTRON)) {
                        field.type = FT_INFOTRON;
                        field.del_hint(HINT_EXPLOSION_INFOTRON);
                    }

                    field.del_hint(HINT_EXPLOSION_ORIGIN);
                }
            }

			if (!field.has_hint(HINT_SKIP)) {
				switch (field.type) {
				case FT_ZONK:
				case FT_INFOTRON:
					fall(field, false);
					break;

				case FT_ORANGE_DISK:
					fall(field, true);
					break;

				case FT_SNIK_SNAK:
				case FT_STARS:
					move_npc(field, dir);
					break;

				default:
					break;
				}
			}
        }

        // Skip is used only for current game step. Clear it for next one.
        for (int i = 0; i < width() * height(); ++i) {
            Field &field = data[i];
            field.del_hint(HINT_SKIP);
        }

        return murphy_alive;
    }

    void dispatch_event(GameEvent event) {
        switch (event) {
            case EVENT_MOVE_UP:
                next_move = DIR_UP;
                break;

            case EVENT_MOVE_DOWN:
                next_move = DIR_DOWN;
                break;

            case EVENT_MOVE_LEFT:
                next_move = DIR_LEFT;
                break;

            case EVENT_MOVE_RIGHT:
                next_move = DIR_RIGHT;
                break;

            case EVENT_MOVE_NONE:
                next_move = DIR_NONE;
                break;

            case EVENT_END_GAME:
                // TODO: Explode Murphy.
                break;

            case EVENT_BTN_SPECIAL:
                // TODO: Handle better
                break;
        }
    }

    int width() const {
        return LEVEL_WIDTH;
    }

    int height() const {
        return LEVEL_HEIGHT;
    }

protected:
    Direction next_move;
    Point murphy;

    Point next_point(Point current, Direction dir) const {
        switch (dir) {
            case DIR_UP:
                if (current.y > 0) {
                    return Point(current.x, current.y - 1);
                } else {
                    return current;
                }
                break;

            case DIR_DOWN:
                if (current.y < height() - 1) {
                    return Point(current.x, current.y + 1);
                } else {
                    return current;
                }
                break;

            case DIR_LEFT:
                if (current.x > 0) {
                    return Point(current.x - 1, current.y);
                } else {
                    return current;
                }
                break;

            case DIR_RIGHT:
                if (current.x < width() - 1) {
                    return Point(current.x + 1, current.y);
                } else {
                    return current;
                }
                break;

            default:
                break;
        }

        return current;
    }

    int data_idx(Point p) const {
        return p.y * width() + p.x;
    }

	unsigned int hint_from_direction(Direction dir) {
		switch (dir) {
		case DIR_NONE:
			return HINT_NONE;
		case DIR_LEFT:
			return HINT_FROM_RIGHT;
		case DIR_RIGHT:
			return HINT_FROM_LEFT;
		case DIR_UP:
			return HINT_FROM_BOTTOM;
		case DIR_DOWN:
			return HINT_FROM_TOP;
		}
	}

	Direction turn_left(const Direction &dir) {
		switch (dir) {
		case DIR_NONE:
			return DIR_NONE;
		case DIR_LEFT:
			return DIR_DOWN;
		case DIR_RIGHT:
			return DIR_UP;
		case DIR_UP:
			return DIR_LEFT;
		case DIR_DOWN:
			return DIR_RIGHT;
		}
	}

	Direction turn_right(const Direction &dir) {
		switch (dir) {
		case DIR_NONE:
			return DIR_NONE;
		case DIR_LEFT:
			return DIR_UP;
		case DIR_RIGHT:
			return DIR_DOWN;
		case DIR_UP:
			return DIR_RIGHT;
		case DIR_DOWN:
			return DIR_LEFT;
		}
	}

	Direction turn_back(const Direction &dir) {
		switch (dir) {
		case DIR_NONE:
			return DIR_NONE;
		case DIR_LEFT:
			return DIR_RIGHT;
		case DIR_RIGHT:
			return DIR_LEFT;
		case DIR_UP:
			return DIR_DOWN;
		case DIR_DOWN:
			return DIR_UP;
		}
	}

    void fall(Field &fld, bool destructive) {
        Point pt_below = next_point(fld.coords, DIR_DOWN);
        Field &below = data[data_idx(pt_below)];

        switch (below.type) {
            case FT_EMPTY:
                if (!below.has_hint(HINT_LEAVING)) {
                    below.type = fld.type;
                    fld.type = FT_EMPTY;
                    below.set_hint(HINT_FALL | HINT_SKIP);
				}
                break;

            case FT_MURPHY:
            case FT_SNIK_SNAK:
            case FT_ORANGE_DISK:
                if (fld.has_hint(HINT_FALL) && (below.type != FT_MURPHY || !below.has_hint(HINT_LEAVING))) {
                    explode_9(below, FT_EMPTY);
                }
                break;

            case FT_STARS:
                if (fld.has_hint(HINT_FALL)) {
                    explode_9(below, FT_INFOTRON);
                }
                break;

            default:
                if (fld.has_hint(HINT_FALL) && destructive) {
                    explode_9(fld, FT_EMPTY);
                } else if (below.rolls_on_impact()) {
                    Field &left = data[data_idx(next_point(fld.coords, DIR_LEFT))];
                    Field &right = data[data_idx(next_point(fld.coords, DIR_RIGHT))];
                    Field &lbelow = data[data_idx(next_point(left.coords, DIR_DOWN))];
                    Field &rbelow = data[data_idx(next_point(right.coords, DIR_DOWN))];

                    // Roll left
                    if (left.type == FT_EMPTY && lbelow.type == FT_EMPTY) {
						if (!left.has_hint(HINT_LEAVING) && !lbelow.has_hint(HINT_LEAVING)) {
							left.type = fld.type;
							fld.type = FT_EMPTY;
							left.set_hint(HINT_FROM_RIGHT | HINT_SKIP);
						}
                    }

                    // Roll right
                    else if (right.type == FT_EMPTY && rbelow.type == FT_EMPTY) {
						if (!right.has_hint(HINT_LEAVING) && !rbelow.has_hint(HINT_LEAVING)) {
							right.type = fld.type;
							fld.type = FT_EMPTY;
							right.set_hint(HINT_FROM_LEFT | HINT_SKIP);
						}
                    }
                }
                break;
        }

        fld.del_hint(HINT_FALL);
    }

    void explode_9(Field &origin, FieldType fill) {
        origin.set_hint(HINT_EXPLOSION_ORIGIN);

        for (int y = origin.coords.y - 1; y <= origin.coords.y + 1; ++y) {
            for (int x = origin.coords.x - 1; x <= origin.coords.x + 1; ++x) {
                Field &fld = data[data_idx(Point(x, y))];
                if (fld.affected_by_explosion()) {
                    if (fill == FT_EMPTY) {
                        fld.set_hint(HINT_EXPLOSION | HINT_SKIP);
                        fld.countdown = EXPLOSION_STEPS;
                    } else if (fill == FT_INFOTRON) {
                        fld.set_hint(HINT_EXPLOSION_INFOTRON | HINT_SKIP);
                        fld.countdown = EXPLOSION_STEPS;
                    }
                }

                if (fld.type == FT_MURPHY) {
                    murphy_alive = false;
                }
            }
        }
    }

    void move_npc(Field &field, Direction dir) {
        // Test whether we can rotate left.
        Point turn_left_pt;
        Point turn_right_pt;

        switch (dir) {
            case DIR_UP:
                turn_left_pt = next_point(field.coords, DIR_LEFT);
                turn_right_pt = next_point(field.coords, DIR_RIGHT);
                break;

            case DIR_DOWN:
                turn_left_pt = next_point(field.coords, DIR_RIGHT);
                turn_right_pt = next_point(field.coords, DIR_LEFT);
                break;

            case DIR_LEFT:
                turn_left_pt = next_point(field.coords, DIR_DOWN);
                turn_right_pt = next_point(field.coords, DIR_UP);
                break;

            case DIR_RIGHT:
                turn_left_pt = next_point(field.coords, DIR_UP);
                turn_right_pt = next_point(field.coords, DIR_DOWN);
                break;

            default:
                break;
        }

        Direction can_turn = DIR_NONE;

        if (!field.has_hint(HINT_TURN_LEFT | HINT_TURN_RIGHT)) {
            Field &turn_left = data[data_idx(turn_left_pt)];
            Field &turn_right = data[data_idx(turn_right_pt)];

            if (turn_left.type == FT_EMPTY && !turn_left.has_hint(HINT_LEAVING)) {
                can_turn = DIR_LEFT;
            } else if (turn_right.type == FT_EMPTY && !turn_right.has_hint(HINT_LEAVING)) {
                can_turn = DIR_RIGHT;
            }
        }

        // Clear current move, because we already now what we are going to do here.
        field.del_hint(HINT_TURN_LEFT | HINT_TURN_RIGHT | HINT_FROM_TOP | HINT_FROM_BOTTOM | HINT_FROM_LEFT | HINT_FROM_RIGHT);

        Point next_pt = next_point(field.coords, dir);
        Field &next = data[data_idx(next_pt)];

        bool moving = false;

        if ((can_turn == DIR_NONE || can_turn == DIR_RIGHT) && !next.has_hint(HINT_LEAVING)) {
            if (next.type == FT_EMPTY) {
                next.type = field.type;
				next.set_hint(hint_from_direction(dir) | HINT_SKIP);

                field.type = FT_EMPTY;
                field.set_hint(HINT_LEAVING);

                moving = true;
            } else if (next.type == FT_MURPHY) {
                explode_9(next, FT_EMPTY);
            }

            next.set_hint(HINT_SKIP);
        }

        if (!moving && can_turn == DIR_LEFT) {
            // Rotate left.
			field.set_hint(hint_from_direction(turn_left(dir)) | HINT_TURN_LEFT);
        } else if (!moving) {
            // Rotate right.
			field.set_hint(hint_from_direction(turn_right(dir)) | HINT_TURN_RIGHT);
        }
    }
};

/**
 * Abstract class that represents UI.
 */
class Drawer {
public:
    virtual ~Drawer() {}

    /**
     * Handle input events. Return true, if the loop should continue, false if the game should be aborted.
     */
    virtual bool handle_input(Level *game) = 0;

    /**
     * Draw game field to the screen.
     */
    virtual void draw(Level *game, int animation_frame) = 0;

    /**
     * Return number of animation frames for each game step.
     */
    virtual int animation_frames() = 0;
};

/**
 * SDL based interface.
 */
class SDLDrawer: public Drawer {
public:
    static const int FIELD_WIDTH = 16;
    static const int FIELD_HEIGHT = 16;

public:
    SDLDrawer(): last_murphy_side_move(DIR_LEFT) {
        SDL_Init(SDL_INIT_VIDEO);
        window = SDL_CreateWindow("Supaplex", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, FIELD_WIDTH * 60, FIELD_HEIGHT * 24, SDL_WINDOW_RESIZABLE);

        fixed = SDL_LoadBMP("FIXED.bmp");
        moving = SDL_LoadBMP("MOVING2.bmp");
    }

    ~SDLDrawer() {
        SDL_FreeSurface(fixed);
        SDL_FreeSurface(moving);

        SDL_DestroyWindow(window);
        SDL_Quit();
    }

    bool handle_input(Level *level) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    return false;

                case SDL_KEYDOWN:
                    switch (event.key.keysym.sym) {
                        case SDLK_UP:
                            if (keyboard_down[KBD_UP] == 0) {
                                keyboard_down[KBD_UP] = 1;
                            }
                            break;

                        case SDLK_DOWN:
                            if (keyboard_down[KBD_DOWN] == 0) {
                                keyboard_down[KBD_DOWN] = 1;
                            }
                            break;

                        case SDLK_LEFT:
                            if (keyboard_down[KBD_LEFT] == 0) {
                                keyboard_down[KBD_LEFT] = 1;
                            }
                            break;

                        case SDLK_RIGHT:
                            if (keyboard_down[KBD_RIGHT] == 0) {
                                keyboard_down[KBD_RIGHT] = 1;
                            }
                            break;

                        case SDLK_SPACE:
                            keyboard_down[KBD_SPACE] = 1;
                            break;
                    }
                    break;

                case SDL_KEYUP:
                    switch (event.key.keysym.sym) {
                        case SDLK_UP:
                            if (keyboard_down[KBD_UP] == 1) {
                                // was up, but only for this round, will go down after processed.
                                keyboard_down[KBD_UP] = 3;
                            } else if (keyboard_down[KBD_UP] == 2) {
                                // was up, goes immediately down.
                                keyboard_down[KBD_UP] = 0;
                            }
                            break;

                        case SDLK_DOWN:
                            if (keyboard_down[KBD_DOWN] == 1) {
                                keyboard_down[KBD_DOWN] = 3;
                            } else if (keyboard_down[KBD_DOWN] == 2) {
                                keyboard_down[KBD_DOWN] = 0;
                            }
                            break;

                        case SDLK_LEFT:
                            if (keyboard_down[KBD_LEFT] == 1) {
                                keyboard_down[KBD_LEFT] = 3;
                            } else if (keyboard_down[KBD_LEFT] == 2) {
                                keyboard_down[KBD_LEFT] = 0;
                            }
                            break;

                        case SDLK_RIGHT:
                            if (keyboard_down[KBD_RIGHT] == 1) {
                                keyboard_down[KBD_RIGHT] = 3;
                            } else if (keyboard_down[KBD_RIGHT] == 2) {
                                keyboard_down[KBD_RIGHT] = 0;
                            }
                            break;

                        case SDLK_SPACE:
                            keyboard_down[KBD_SPACE] = 0;
                            break;
                    }
                    break;
            }
        }

        if (keyboard_down[KBD_UP] > 0) {
            level->dispatch_event(EVENT_MOVE_UP);
        } else if (keyboard_down[KBD_DOWN] > 0) {
            level->dispatch_event(EVENT_MOVE_DOWN);
        } else if (keyboard_down[KBD_LEFT] > 0) {
            level->dispatch_event(EVENT_MOVE_LEFT);
        } else if (keyboard_down[KBD_RIGHT] > 0) {
            level->dispatch_event(EVENT_MOVE_RIGHT);
        } else {
            level->dispatch_event(EVENT_MOVE_NONE);
        }

        if (keyboard_down[KBD_UP] == 3) {
            keyboard_down[KBD_UP] = 0;
        } else if (keyboard_down[KBD_UP] == 1) {
            keyboard_down[KBD_UP] = 2;
        }

        if (keyboard_down[KBD_DOWN] == 3) {
            keyboard_down[KBD_DOWN] = 0;
        } else if (keyboard_down[KBD_DOWN] == 1) {
            keyboard_down[KBD_DOWN] = 2;
        }

        if (keyboard_down[KBD_LEFT] == 3) {
            keyboard_down[KBD_LEFT] = 0;
        } else if (keyboard_down[KBD_LEFT] == 1) {
            keyboard_down[KBD_LEFT] = 2;
        }

        if (keyboard_down[KBD_RIGHT] == 3) {
            keyboard_down[KBD_RIGHT] = 0;
        } else if (keyboard_down[KBD_RIGHT] == 1) {
            keyboard_down[KBD_RIGHT] = 2;
        }

        return true;
    }

    void draw(Level *level, int animation_frame) {
        SDL_Surface *screen = SDL_GetWindowSurface(window);

        SDL_Rect source;
        source.x = 0;
        source.y = 0;
        source.w = FIELD_WIDTH;
        source.h = FIELD_HEIGHT;

        SDL_Rect dest;
        dest.x = 0;
        dest.y = 0;
        dest.w = FIELD_WIDTH;
        dest.h = FIELD_HEIGHT;

        SDL_Rect source_empty;
        source_empty.x = 0;
        source_empty.y = 0;
        source_empty.w = FIELD_WIDTH;
        source_empty.h = FIELD_HEIGHT;

        SDL_Rect source_infotron = source_empty;
        source_infotron.x = FIELD_WIDTH * FT_INFOTRON;

        SDL_Rect source_base = source_empty;
        source_base.x = FIELD_WIDTH * FT_BASE;

        SDL_Rect source_red_disk = source_empty;
        source_red_disk.x = FIELD_WIDTH * FT_RED_DISK;

        SDL_Surface *source_surface;

        int move_offset = FIELD_HEIGHT / animation_frames();

        // Draw static fields.
        for (int ly = 0; ly < level->height(); ++ly) {
            for (int lx = 0; lx < level->width(); ++lx) {
                dest.y = ly * FIELD_HEIGHT;
                dest.x = lx * FIELD_WIDTH;

                Field &field = level->data[ly * level->width() + lx];

                source.y = 0;
                source.x = field.type * FIELD_WIDTH;

                SDL_BlitSurface(fixed, &source, screen, &dest);
            }
        }

        for (int ly = 0; ly < level->height(); ++ly) {
            for (int lx = 0; lx < level->width(); ++lx) {
                dest.y = ly * FIELD_HEIGHT;
                dest.x = lx * FIELD_WIDTH;

                source_surface = fixed;

                bool need_draw = false;

                Field &field = level->data[ly * level->width() + lx];

                source.y = 0;
                source.x = field.type * FIELD_WIDTH;

                if (field.has_hint(HINT_FALL) || field.has_hint(HINT_FROM_TOP) || field.has_hint(HINT_FROM_BOTTOM)
                        || field.has_hint(HINT_FROM_LEFT) || field.has_hint(HINT_FROM_RIGHT)) {

                    if (field.has_hint(HINT_WAS_INFOTRON)) {
                        SDL_BlitSurface(fixed, &source_infotron, screen, &dest);
                    } else if (field.has_hint(HINT_WAS_BASE)) {
                        SDL_BlitSurface(fixed, &source_base, screen, &dest);
                    } else if (field.has_hint(HINT_WAS_RED_DISK)) {
                        SDL_BlitSurface(fixed, &source_red_disk, screen, &dest);
                    } else {
                        SDL_BlitSurface(fixed, &source_empty, screen, &dest);
                    }

                    need_draw = true;
                }

                if (field.has_hint(HINT_FALL)) {
                    dest.y = dest.y - FIELD_HEIGHT + move_offset * animation_frame;
                } else if (field.has_hint(HINT_FROM_TOP) && !field.has_hint(HINT_TURN_LEFT | HINT_TURN_RIGHT)) {
                    dest.y = dest.y - FIELD_HEIGHT + move_offset * animation_frame;
                } else if (field.has_hint(HINT_FROM_BOTTOM) && !field.has_hint(HINT_TURN_LEFT | HINT_TURN_RIGHT)) {
                    dest.y = dest.y + FIELD_HEIGHT - move_offset * animation_frame;
                } else if (field.has_hint(HINT_FROM_LEFT) && !field.has_hint(HINT_TURN_LEFT | HINT_TURN_RIGHT)) {
                    dest.x = dest.x - FIELD_HEIGHT + move_offset * animation_frame;
                } else if (field.has_hint(HINT_FROM_RIGHT) && !field.has_hint(HINT_TURN_LEFT | HINT_TURN_RIGHT)) {
                    dest.x = dest.x + FIELD_HEIGHT - move_offset * animation_frame;
                }

                if (has_animation(field)) {
                    source_surface = moving;
                    need_draw = true;

                    int turn_offset = 0;
                    if (field.has_hint(HINT_TURN_LEFT)) {
                        turn_offset = 4;
                    } else if (field.has_hint(HINT_TURN_RIGHT)) {
                        turn_offset = 8;
                    }

                    if (field.has_hint(HINT_EXPLOSION)) {
                        // Extend explosion animation to 4 game steps.
                        source.x = ((animation_frame >> 2) + ((EXPLOSION_STEPS - field.countdown) << 1)) * FIELD_WIDTH;
                        source.y = 6 * FIELD_HEIGHT;
                    }

                    else if (field.has_hint(HINT_FROM_LEFT)
                            || (field.type == FT_MURPHY && last_murphy_side_move == DIR_RIGHT &&
                                field.has_hint(HINT_FROM_TOP | HINT_FROM_BOTTOM)))
                    {
                        source.x = animation_frame * FIELD_WIDTH;

                        switch (field.type) {
                            case FT_MURPHY:
								if (!field.has_hint(HINT_PUSH)) {
									source.y = 1 * FIELD_HEIGHT;
									last_murphy_side_move = DIR_RIGHT;
								}
								else {
									source.x = 0 * FIELD_WIDTH;
									source.y = 20 * FIELD_HEIGHT;
								}
                                break;

                            case FT_ZONK:
                                source.y = 3 * FIELD_HEIGHT;
                                break;

                            case FT_INFOTRON:
                                source.y = 5 * FIELD_HEIGHT;
                                break;

                            case FT_SNIK_SNAK:
                                source.y = (9 + turn_offset) * FIELD_HEIGHT;
                                break;

                            case FT_STARS:
                                break;

                            default:
                                break;
                        }
                    }

                    else if (field.has_hint(HINT_FROM_RIGHT)
                            || (field.type == FT_MURPHY && last_murphy_side_move == DIR_LEFT &&
                                field.has_hint(HINT_FROM_TOP | HINT_FROM_BOTTOM)))
                    {
                        source.x = animation_frame * FIELD_WIDTH;

                        switch (field.type) {
                            case FT_MURPHY:
								if (!field.has_hint(HINT_PUSH)) {
									source.y = 0 * FIELD_HEIGHT;
									last_murphy_side_move = DIR_LEFT;
								}
								else {
									source.x = 1 * FIELD_WIDTH;
									source.y = 20 * FIELD_HEIGHT;
								}
                                break;

                            case FT_ZONK:
                                source.y = 2 * FIELD_HEIGHT;
                                break;

                            case FT_INFOTRON:
                                source.y = 4 * FIELD_HEIGHT;
                                break;

                            case FT_SNIK_SNAK:
                                source.y = (8 + turn_offset) * FIELD_HEIGHT;
                                break;

                            default:
                                break;
                        }
                    }

                    else if (field.has_hint(HINT_FROM_TOP)) {
                        source.x = animation_frame * FIELD_WIDTH;

                        switch (field.type) {
                            case FT_SNIK_SNAK:
                                source.y = (11 + turn_offset) * FIELD_HEIGHT;
                                break;

                            default:
                                break;
                        }
                    }

                    else if (field.has_hint(HINT_FROM_BOTTOM)) {
                        source.x = animation_frame * FIELD_WIDTH;

                        switch (field.type) {
                            case FT_SNIK_SNAK:
                                source.y = (10 + turn_offset) * FIELD_HEIGHT;
                                break;

                            default:
                                break;
                        }
                    }
                }

                if (need_draw) {
                    SDL_BlitSurface(source_surface, &source, screen, &dest);
                }
            }
        }

        SDL_UpdateWindowSurface(window);
    }

    int animation_frames() {
        return 8;
    }

protected:
    static const int KBD_UP = 0;
    static const int KBD_DOWN = 1;
    static const int KBD_LEFT = 2;
    static const int KBD_RIGHT = 3;
    static const int KBD_SPACE = 4;

    SDL_Window *window;
    SDL_Surface *fixed;
    SDL_Surface *moving;

    int keyboard_down[5];
    Direction last_murphy_side_move;

    bool has_animation(Field field) {
        if (field.has_hint(HINT_EXPLOSION)
            || field.has_hint(HINT_FROM_LEFT | HINT_FROM_RIGHT | HINT_FROM_TOP | HINT_FROM_BOTTOM))
        {
            return true;
        }

        return false;
    }
};

int main(int argc, char **argv) {
    Level *level = new Level("LEVELS.DAT", 1);
    Drawer *drawer = new SDLDrawer();

    bool cont = true;
    time_t level_start = time(NULL) + 2;
    int animation_frame = 0;

	std::chrono::time_point<std::chrono::high_resolution_clock> tm_start;
	std::chrono::time_point<std::chrono::high_resolution_clock> tm_end;

    while (cont) {
		tm_start = std::chrono::high_resolution_clock::now();

        int64_t level_time = time(NULL) - level_start;
        if (level_time >= 0) {
            if (animation_frame == 0) {
                cont &= drawer->handle_input(level);
                cont &= level->game_step();
            }
        }

        drawer->draw(level, animation_frame);

        animation_frame = (animation_frame + 1) % drawer->animation_frames();

		tm_end = std::chrono::high_resolution_clock::now();
		auto lasted = std::chrono::duration_cast<std::chrono::milliseconds>(tm_end - tm_start);
		std::this_thread::sleep_for(std::chrono::milliseconds(1000) / FPS - lasted);
    }

    return EXIT_SUCCESS;
}