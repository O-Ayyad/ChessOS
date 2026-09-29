#include "board_view.h"
#include "math_utils.h"
#include "memory.h"
#include "sound.h"
#include "chess_rules.h"

// The board covers 0 to 8 in x and y and a frame around it.
const float FRAME_MIN = -0.6f;
const float FRAME_MAX = 8.6f;
const float NO_HIT = 1e9f;

const u32 COLOR_LIGHT_SQUARE_DOT = 0x333333;
const u32 COLOR_GRID_LINE = 0x4A4A4A;
const u32 COLOR_BOARD_EDGE = 0x8A8A8A;
const u32 COLOR_FRAME = 0x5A5A5A;
const u32 COLOR_HOVER = 0x505050;


const float FIRST_VIEW_ANGLE = 270;
const float ANGLE_BETWEEN_VIEWS = 45;
const float TOP_VIEW_ELEVATION = 89.5f;
const float CORNER_VIEW_ELEVATION = 38;
const float CORNER_VIEW_DISTANCE = 14;
const float TOP_VIEW_DISTANCE = 60;
static const char* VIEW_NAMES[VIEW_COUNT] = {
    "TOP / WHITE", 
    "CORNER 1", 
    "TOP / SIDE", 
    "CORNER 2", 
    "TOP / BLACK", 
    "CORNER 3", 
    "TOP / SIDE", 
    "CORNER 4"
};

struct Camera {
    bool looks_straight_down;
    Vector3 position;
    Vector3 forward, right, up;
    float zoom; //pixels per unit
    float center_x, center_y; // where the board centre lands in the view
    float angle_degrees; // the direction we look from
};

static Camera camera;
static int view_number = 0;
static PiecePictures* piece_pictures = nullptr;
static Image view_image;
static float* board_hit_x = nullptr; //where each pixel ray hits the board
static float* board_hit_y = nullptr;
static bool hits_need_update = true;

static void project(Vector3 p, float& screen_x, float& screen_y, float& distance) {
    Vector3 offset = subtract_vectors(p, camera.position);
    distance = dot_product(offset, camera.forward);
    float scale = camera.looks_straight_down ? camera.zoom : camera.zoom / distance;
    screen_x = camera.center_x + dot_product(offset, camera.right) * scale;
    screen_y = camera.center_y - dot_product(offset, camera.up) * scale;
}

static void ray_through_pixel(float x, float y, Vector3& origin, Vector3& direction) {
    float a = (x - camera.center_x) / camera.zoom;
    float b = (y - camera.center_y) / camera.zoom;
    Vector3 sideways = subtract_vectors(scale_vector(camera.right, a), scale_vector(camera.up, b));
    if (camera.looks_straight_down) {
        origin = add_vectors(camera.position, sideways);
        direction = camera.forward;
    } else {
        origin = camera.position;
        direction = normalize_vector(add_vectors(camera.forward, sideways));
    }
}

void set_board_view(int number) {
    view_number = ((number % VIEW_COUNT) + VIEW_COUNT) % VIEW_COUNT;
    bool top_view = view_number % 2 == 0;
    camera.angle_degrees = FIRST_VIEW_ANGLE + ANGLE_BETWEEN_VIEWS * view_number;
    float angle = degrees_to_radians(camera.angle_degrees);
    float elevation = degrees_to_radians(top_view ? TOP_VIEW_ELEVATION : CORNER_VIEW_ELEVATION);

    Vector3 towards_camera = make_vector(cosine(elevation) * cosine(angle), cosine(elevation) * sine(angle), sine(elevation));
    Vector3 board_center = make_vector(4, 4, 0);
    camera.looks_straight_down = top_view;
    camera.position = add_vectors(board_center, scale_vector(towards_camera, top_view ? TOP_VIEW_DISTANCE : CORNER_VIEW_DISTANCE));
    camera.forward = scale_vector(towards_camera, -1);
    camera.right = normalize_vector(cross_product(camera.forward, make_vector(0, 0, 1)));
    camera.up = cross_product(camera.right, camera.forward);

    camera.zoom = 1;
    camera.center_x = 0;
    camera.center_y = 0;
    float left = NO_HIT, top = NO_HIT, right = -NO_HIT, bottom = -NO_HIT;
    Vector3 corners[8];
    int corner_count = 0;
    for (int i = 0; i < 4; i++) {
        corners[corner_count++] = make_vector((i & 1) ? FRAME_MAX : FRAME_MIN, (i & 2) ? FRAME_MAX : FRAME_MIN, 0);
    }
    if (!top_view) {
        const float PIECE_TOP_HEIGHT = 1.15f;
        for (int i = 0; i < 4; i++) {
            corners[corner_count++] = make_vector((i & 1) ? 7.6f : 0.4f, (i & 2) ? 7.6f : 0.4f, PIECE_TOP_HEIGHT);
        }
    }
    for (int i = 0; i < corner_count; i++) {
        float x, y, distance;
        project(corners[i], x, y, distance);
        left = min_float(left, x);
        right = max_float(right, x);
        top = min_float(top, y);
        bottom = max_float(bottom, y);
    }
    float margin = top_view ? 0.96f : 0.97f;
    camera.zoom = min_float(BOARD_VIEW_WIDTH / (right - left), BOARD_VIEW_HEIGHT / (bottom - top)) * margin;
    camera.center_x = BOARD_VIEW_WIDTH / 2.0f - (left + right) / 2 * camera.zoom;
    camera.center_y = BOARD_VIEW_HEIGHT / 2.0f - (top + bottom) / 2 * camera.zoom;
    hits_need_update = true;
}

int current_board_view() { return view_number; }
const char* board_view_name() { return VIEW_NAMES[view_number]; }
Image& board_view_image() { return view_image; }

void setup_board_view(PiecePictures* pictures) {
    piece_pictures = pictures;
    view_image = create_image(BOARD_VIEW_WIDTH, BOARD_VIEW_HEIGHT);
    board_hit_x = allocate_array<float>(BOARD_VIEW_WIDTH * BOARD_VIEW_HEIGHT);
    board_hit_y = allocate_array<float>(BOARD_VIEW_WIDTH * BOARD_VIEW_HEIGHT);
    set_board_view(0);
}

static void update_board_hits() {
    for (int y = 0; y < BOARD_VIEW_HEIGHT; y++) {
        for (int x = 0; x < BOARD_VIEW_WIDTH; x++) {
            Vector3 origin, direction;
            ray_through_pixel(x + 0.5f, y + 0.5f, origin, direction);
            float hit_x = NO_HIT, hit_y = NO_HIT;
            if (direction.z < -0.000001f) {
                float travel = -origin.z / direction.z;
                hit_x = origin.x + direction.x * travel;
                hit_y = origin.y + direction.y * travel;
            }
            board_hit_x[y * BOARD_VIEW_WIDTH + x] = hit_x;
            board_hit_y[y * BOARD_VIEW_WIDTH + x] = hit_y;
        }
    }
    hits_need_update = false;
}


static int whole_square(float value) {
    return (int)round_down(value);
}

static int make_square_index(float x, float y) {
    return (int)y * 8 + (int)x;
}

static bool crosses(float a, float b, float line_value) {
    return (a < line_value) != (b < line_value);
}

static u32 board_pixel_color(const BoardViewState& state, int x, int y) {
    int index = y * BOARD_VIEW_WIDTH + x;
    float hx = board_hit_x[index];
    float hy = board_hit_y[index];
    const float SLACK = 0.05f;
    if (hx < FRAME_MIN - SLACK || hx > FRAME_MAX + SLACK || hy < FRAME_MIN - SLACK || hy > FRAME_MAX + SLACK) {
        return COLOR_BLACK;  // outside the board and frame
    }
    float left_x = x > 0 ? board_hit_x[index - 1] : hx;
    float left_y = x > 0 ? board_hit_y[index - 1] : hy;
    float above_x = y > 0 ? board_hit_x[index - BOARD_VIEW_WIDTH] : hx;
    float above_y = y > 0 ? board_hit_y[index - BOARD_VIEW_WIDTH] : hy;

    bool on_line = false;
    bool on_outer_edge = false;
    if (whole_square(hx) != whole_square(left_x) || whole_square(hx) != whole_square(above_x)) {
        int line = max_int(whole_square(hx), max_int(whole_square(left_x), whole_square(above_x)));
        if (line >= 0 && line <= 8 && hy >= 0 && hy <= 8) {
            on_line = true;
            on_outer_edge = line == 0 || line == 8;
        }
    }
    if (whole_square(hy) != whole_square(above_y) || whole_square(hy) != whole_square(left_y)) {
        int line = max_int(whole_square(hy), max_int(whole_square(above_y), whole_square(left_y)));
        if (line >= 0 && line <= 8 && hx >= 0 && hx <= 8) {
            on_line = true;
            on_outer_edge = on_outer_edge || line == 0 || line == 8;
        }
    }
    bool on_frame = ((crosses(hx, left_x, FRAME_MIN) || crosses(hx, above_x, FRAME_MIN) ||
                      crosses(hx, left_x, FRAME_MAX) || crosses(hx, above_x, FRAME_MAX)) && hy >= FRAME_MIN && hy <= FRAME_MAX) ||
                    ((crosses(hy, above_y, FRAME_MIN) || crosses(hy, left_y, FRAME_MIN) ||
                      crosses(hy, above_y, FRAME_MAX) || crosses(hy, left_y, FRAME_MAX)) && hx >= FRAME_MIN && hx <= FRAME_MAX);
    if (on_frame) return COLOR_FRAME;

    bool inside_board = hx >= 0 && hx < 8 && hy >= 0 && hy < 8;
    int square = inside_board ? make_square_index(hx, hy) : -1;
    if (on_line) {
        if (inside_board && square == state.selected_square) return COLOR_LIGHT_CYAN;
        return on_outer_edge ? COLOR_BOARD_EDGE : COLOR_GRID_LINE;
    }
    if (!inside_board) return COLOR_BLACK;

    u32 color = COLOR_BLACK;
    int file = (int)hx, rank = (int)hy;
    bool light_square = (file + rank) % 2 == 1;

    bool dot_pattern = x % 2 == 0 && y % 2 == 0 && ((x / 2 + y / 2) % 2 == 1);
    if (light_square && dot_pattern) color = COLOR_LIGHT_SQUARE_DOT;

    //Orange highlight for last move
    bool light_pattern = x % 2 == 0 && y % 2 == 0;
    bool dense_pattern = (x + y) % 2 == 0;
    if ((square == state.last_move_from || square == state.last_move_to) && light_pattern) color = COLOR_BROWN;
    if (square == state.hovered_square && light_pattern) color = COLOR_HOVER;
    if (square == state.selected_square && dense_pattern) color = COLOR_CYAN;
    if (square == state.king_in_check_square && dense_pattern) color = COLOR_RED;

    // a green dot in the middle for legal moves and red for capture
    if (state.target_squares & (1ULL << square)) {
        float dx = (hx - file) - 0.5f;
        float dy = (hy - rank) - 0.5f;
        float distance_from_middle = square_root(dx * dx + dy * dy);
        if (state.squares[square] != 0) {
            if (distance_from_middle > 0.40f && distance_from_middle < 0.47f) color = COLOR_LIGHT_RED;
        } else if (distance_from_middle < 0.11f) {
            color = COLOR_LIGHT_GREEN;
        }
    }
    return color;
}

static void draw_coordinate_labels() {
    for (int i = 0; i < 8; i++) {
        float x, y, distance;
        project(make_vector(i + 0.5f, -0.3f, 0), x, y, distance);          // a..h below the board
        draw_character(view_image, (int)x - CHAR_WIDTH / 2, (int)y - CHAR_HEIGHT / 2, 'a' + i, COLOR_DARK_GRAY);
        project(make_vector(-0.3f, i + 0.5f, 0), x, y, distance);          // 1..8 beside it
        draw_character(view_image, (int)x - CHAR_WIDTH / 2, (int)y - CHAR_HEIGHT / 2, '1' + i, COLOR_DARK_GRAY);
    }
}


static const float SIZE_SCALE[PIECE_SIZE_COUNT] = { 0.45f, 0.62f, 0.80f };
const int PIECE_BASE_X = 96;
const int PIECE_BASE_Y = 176;
const float DEGREES_PER_ROTATION = 360.0f / PIECE_ROTATIONS;
const float PIXELS_PER_SQUARE_AT_SCALE_1 = 100.0f;
const float TOP_VIEW_PIECE_SHIFT = 0.36f;
const float TOP_VIEW_SIZE_BOOST = 1.05f;
const float CORNER_VIEW_SIZE_BOOST = 1.2f;
const int MAX_PIECES = 40;

struct PieceToDraw {
    int piece;
    float board_x, board_y; 
    float depth; 
};

struct DrawnPiece {
    int square;
    int x, y; 
    const Bitmap* picture;
    const VisibleArea* visible;
};
static DrawnPiece drawn_pieces[MAX_PIECES];
static int drawn_piece_count = 0;

static void draw_piece(const PieceToDraw& piece) {
    float screen_x, screen_y, distance;
    project(make_vector(piece.board_x, piece.board_y, 0), screen_x, screen_y, distance);

    float wanted_scale = (camera.looks_straight_down ? camera.zoom : camera.zoom / distance) / PIXELS_PER_SQUARE_AT_SCALE_1;
    float looking_from_angle;
    if (camera.looks_straight_down) {
        screen_y += TOP_VIEW_PIECE_SHIFT * camera.zoom;
        wanted_scale *= TOP_VIEW_SIZE_BOOST;
        looking_from_angle = camera.angle_degrees;
    } else {
        wanted_scale *= CORNER_VIEW_SIZE_BOOST;
        looking_from_angle = radians_to_degrees(arc_tangent2(camera.position.y - piece.board_y, camera.position.x - piece.board_x));
    }

    int size = 0;
    float best_difference = NO_HIT;
    for (int i = 0; i < PIECE_SIZE_COUNT; i++) {
        float difference = abs_float(SIZE_SCALE[i] / wanted_scale - 1);
        if (difference < best_difference) {
            best_difference = difference;
            size = i;
        }
    }
    int color = piece_color(piece.piece);
    int type = piece_type(piece.piece);

    const float WHITE_KNIGHT_FACING = 180;
    const float BLACK_KNIGHT_FACING = 0;
    float facing = 0;
    if (type == KNIGHT) facing = color == WHITE ? WHITE_KNIGHT_FACING : BLACK_KNIGHT_FACING;
    int rotation = (int)round_down((looking_from_angle - facing) / DEGREES_PER_ROTATION + 0.5f);
    rotation = ((rotation % PIECE_ROTATIONS) + PIECE_ROTATIONS) % PIECE_ROTATIONS;
    const Bitmap& picture = piece_pictures->picture[color][type][size][rotation];
    const VisibleArea& visible = piece_pictures->visible[color][type][size][rotation];

    int left = (int)(screen_x - PIECE_BASE_X + 0.5f);
    int top = (int)(screen_y - PIECE_BASE_Y + 0.5f);
    for (int row = visible.top; row < visible.bottom; row++) {
        int y = top + row;
        if (y < 0 || y >= BOARD_VIEW_HEIGHT) continue;
        u32* target = view_image.pixels + y * BOARD_VIEW_WIDTH;
        for (int column = visible.left; column < visible.right; column++) {
            int x = left + column;
            if (x < 0 || x >= BOARD_VIEW_WIDTH) continue;
            u32 pixel = bitmap_pixel(picture, column, row);
            if (pixel_alpha(pixel) != 0) target[x] = pixel_color(pixel);
        }
    }

    if (drawn_piece_count < MAX_PIECES) {
        DrawnPiece& drawn = drawn_pieces[drawn_piece_count];
        drawn.square = make_square_index(piece.board_x, piece.board_y);
        drawn.x = left;
        drawn.y = top;
        drawn.picture = &picture;
        drawn.visible = &visible;
        drawn_piece_count++;
    }
}

static void draw_all_pieces(const BoardViewState& state) {
    PieceToDraw pieces[MAX_PIECES];
    int count = 0;
    for (int square = 0; square < SQUARE_COUNT; square++) {
        int piece = state.squares[square];
        if (piece == 0) continue;
        if (state.animating && square == state.animation_to) continue;    // drawn sliding instead
        pieces[count].piece = piece;
        pieces[count].board_x = square % 8 + 0.5f;
        pieces[count].board_y = square / 8 + 0.5f;
        count++;
    }
    if (state.animating) {
        float t = state.animation_progress;
        t = t * t * (3 - 2 * t);
        float from_x = state.animation_from % 8 + 0.5f, from_y = state.animation_from / 8 + 0.5f;
        float to_x = state.animation_to % 8 + 0.5f, to_y = state.animation_to / 8 + 0.5f;
        pieces[count].piece = state.animation_piece;
        pieces[count].board_x = from_x + (to_x - from_x) * t;
        pieces[count].board_y = from_y + (to_y - from_y) * t;
        count++;
    }

    for (int i = 0; i < count; i++) {
        float x, y, distance;
        project(make_vector(pieces[i].board_x, pieces[i].board_y, 0), x, y, distance);
        pieces[i].depth = camera.looks_straight_down ? -y : distance;    // top views: higher on screen = farther
    }
    
    
    for (int i = 1; i < count; i++) {
        PieceToDraw current = pieces[i];
        int j = i - 1;
        while (j >= 0 && pieces[j].depth < current.depth) {
            pieces[j + 1] = pieces[j];
            j--;
        }
        pieces[j + 1] = current;
    }

    drawn_piece_count = 0;
    for (int i = 0; i < count; i++) {
        draw_piece(pieces[i]);
    }
}

void draw_board(const BoardViewState& state) {
    if (hits_need_update) update_board_hits();
    for (int y = 0; y < BOARD_VIEW_HEIGHT; y++) {
        const int ROWS_BETWEEN_SOUND_UPDATES = 32;
        if (y % ROWS_BETWEEN_SOUND_UPDATES == 0) update_sound();
        u32* row = view_image.pixels + y * BOARD_VIEW_WIDTH;
        for (int x = 0; x < BOARD_VIEW_WIDTH; x++) {
            row[x] = board_pixel_color(state, x, y);
        }
    }
    draw_coordinate_labels();
    draw_all_pieces(state);
}

int square_at_view_position(int x, int y) {
    //First check the pieces
    for (int i = drawn_piece_count - 1; i >= 0; i--) {
        const DrawnPiece& piece = drawn_pieces[i];
        int u = x - piece.x;
        int v = y - piece.y;
        const VisibleArea& area = *piece.visible;
        if (u < area.left || v < area.top || u >= area.right || v >= area.bottom) continue;
        u32 pixel = bitmap_pixel(*piece.picture, u, v);
        if (pixel_alpha(pixel) != 0) return piece.square;
    }
    
    // otherwise the square of the board under the mouse
    if (x < 0 || y < 0 || x >= BOARD_VIEW_WIDTH || y >= BOARD_VIEW_HEIGHT) return -1;
    float hx = board_hit_x[y * BOARD_VIEW_WIDTH + x];
    float hy = board_hit_y[y * BOARD_VIEW_WIDTH + x];
    if (hx < 0 || hy < 0 || hx >= 8 || hy >= 8) return -1;
    return make_square_index(hx, hy);
}
