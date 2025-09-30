#define _XOPEN_SOURCE 700
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <mutex>
#include <ncurses.h>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

constexpr int MAX_BULLETS = 64;
constexpr int MAX_ENEMIES = 32;
constexpr int MAX_GAME_LEVELS = 3;

constexpr float PLAYER_MOVEMENT_SPEED = 1.2f;
constexpr float PLAYER_BULLET_SPEED = 0.8f;
constexpr float ENEMY_BULLET_SPEED = 0.6f;
constexpr int BASE_ENEMY_ROWS = 2;
constexpr int ENEMY_COLUMNS = 8;
constexpr int ENEMY_MOVEMENT_INTERVAL = 8;
constexpr int ENEMY_SHOOTING_PROBABILITY =
    6; // Probabilidad de que dispare un enemigo.
constexpr int ENEMY_SHOOTING_DENOMINATOR = 1000;
constexpr int UPDATE_INTERVAL_MS = 30; // Intervalo de actualización del juego
constexpr int RENDER_INTERVAL_MS = 25;
constexpr int INPUT_INTERVAL_MS = 10; // Intervalo de input del usuario
constexpr int MOVEMENT_TIMEOUT_MS =
    80; // Tiempo en el que se continua movimiento después de última tecla
constexpr int DAMAGE_FLASH_DURATION_MS = 500;

struct Bullet {
  float x = 0, y = 0;
  bool active = false;
};

struct Enemy {
  float x = 0, y = 0;
  bool alive = false;
  int row = 0;
};

struct EnemyBullet {
  float x = 0, y = 0;
  bool active = false;
};

static int screen_w, screen_h;

// Esta variable sirve para limitar que tanto bajan los enemigos en la pantalla.
static int MAX_ENEMY_Y = 0;

static std::atomic<bool> game_running{true};
static int player_score = 0;
static int player_lives = 3;
static int current_level = 1;
static bool game_completed = false;
static std::atomic<bool> player_hit{false};
static long long damage_flash_start_ms = 0;

// Sincronización de hilos
static std::mutex game_state_mutex;
static std::mutex bullet_mutex;
static std::mutex enemy_mutex;
static std::mutex score_mutex;
static std::atomic<int> semaphore_bullets{1};
static std::atomic<int> semaphore_enemies{1};

static std::condition_variable cv_game_state;
static std::condition_variable cv_level_complete;

static float ship_fx;
static int ship_x, ship_y;
static Bullet bullets[MAX_BULLETS];
static Enemy enemies[MAX_ENEMIES];
static EnemyBullet ebullets[MAX_BULLETS];

// Sprites de los enemigos y del jugador.
static const int SHIP_W = 7;
static const int SHIP_H = 1;
static const char *SHIP_ART[SHIP_H] = {"<( ^ )>"};

static const int ENEMY_W = 5;
static const int ENEMY_H = 2;

static const char *ENEMY_ART_LVL1[ENEMY_H] = {"\\-O-/", "  v  "};
static const char *ENEMY_ART_LVL2[ENEMY_H] = {" /-\\ ", " \\v/ "};
static const char *ENEMY_ART_LVL3[ENEMY_H] = {" /-\\ ", " \\_/ "};

static inline const char **enemy_art_for_level(int lvl) {
  if (lvl <= 1)
    return (const char **)ENEMY_ART_LVL1;
  if (lvl == 2)
    return (const char **)ENEMY_ART_LVL2;
  return (const char **)ENEMY_ART_LVL3;
}

// Para recibir el input del usuario
static std::atomic<bool> move_left{false};
static std::atomic<bool> move_right{false};
static std::atomic<bool> want_fire{false};
// Se guarda el ultimo evento de input como milisegundos.
static std::atomic<long long> last_move_ms{0};

static inline long long now_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

// Si el usuario esta manteniendo presionada la tecla.
static std::atomic<int> held_key{0};

// Buffer para que la pantalla no parpadee.
static WINDOW *backwin = nullptr;

static std::atomic<bool> enemy_stop_descent{false};

// Para guardar los puntajes mas altos
static int saved_highscore = 0;
static const char *HIGHSCORE_FILENAME = "galaga_highscores.txt";

static std::string highscore_path() {
  // Guardar en el folder actual
  return std::string(HIGHSCORE_FILENAME);
}

static std::vector<int> load_highscores() {
  std::vector<int> res(3, 0);
  std::string p = highscore_path();
  std::ifstream in(p);
  if (!in)
    return res;
  for (int i = 0; i < 3 && in; i++) {
    int v;
    in >> v;
    if (in)
      res[i] = v;
  }
  return res;
}

static void save_highscores(const std::vector<int> &hs) {
  std::string p = highscore_path();
  std::ofstream out(p, std::ios::trunc);
  if (!out)
    return;
  for (int i = 0; i < 3; i++)
    out << hs[i] << "\n";
}

// Se guarda si se alcanzo un puntaje mas alto de los existentes.
static void update_highscores_if_needed(int sc) {
  auto hs = load_highscores();
  for (int i = 0; i < 3; i++) {
    if (sc > hs[i]) {
      for (int j = 2; j > i; j--)
        hs[j] = hs[j - 1];
      hs[i] = sc;
      save_highscores(hs);
      saved_highscore = hs[0];
      return;
    }
  }
  saved_highscore = hs[0];
}

// Inicializa el estado del juego y configura la pantalla

void init_game() {
  getmaxyx(stdscr, screen_h, screen_w);
  clear();
  refresh();
  ship_x = screen_w / 2;
  ship_y = std::max(3, screen_h - SHIP_H - 1);
  for (int i = 0; i < MAX_BULLETS; i++)
    bullets[i] = Bullet{};
  for (int i = 0; i < MAX_ENEMIES; i++)
    enemies[i] = Enemy{};
  for (int i = 0; i < MAX_BULLETS; i++)
    ebullets[i] = EnemyBullet{};
  ship_fx = static_cast<float>(screen_w) / 2.0f;
  ship_x = static_cast<int>(std::round(ship_fx));
  std::srand(static_cast<unsigned>(std::time(nullptr)));
  player_score = 0;
  player_lives = 3;
  current_level = 1;
  game_completed = false;
  game_running = true;
  player_hit = false;
  enemy_stop_descent.store(false);
  // Crear buffer fuera de pantalla
  if (backwin) {
    delwin(backwin);
    backwin = nullptr;
  }
  backwin = newwin(screen_h, screen_w, 0, 0);
  // Calcular que tanto pueden bajar los enemigos.
  MAX_ENEMY_Y = std::max(2, screen_h / 2 - ENEMY_H);
}

// Genera enemigos en formación para el nivel especificado

void spawn_enemies(int lvl) {
  int rows = BASE_ENEMY_ROWS + lvl / 2;
  int cols = ENEMY_COLUMNS;
  int idx = 0;
  for (int r = 0; r < rows && idx < MAX_ENEMIES; r++) {
    for (int c = 0; c < cols && idx < MAX_ENEMIES; c++) {
      enemies[idx].alive = true;
      enemies[idx].x = 2 + c * static_cast<float>((screen_w - 4)) / cols;
      enemies[idx].y = 2 + r * (ENEMY_H + 1);
      idx++;
    }
  }
}

// Reinicia el nivel actual

void reset_level() {
  for (int i = 0; i < MAX_BULLETS; i++) {
    bullets[i].active = false;
    bullets[i].x = 0;
    bullets[i].y = 0;
    ebullets[i].active = false;
    ebullets[i].x = 0;
    ebullets[i].y = 0;
  }
  spawn_enemies(current_level);
  enemy_stop_descent.store(false);
}

/**
 * Hilo 1: Manejo de balas del jugador
 * Actualiza la posición de las balas del jugador y elimina las que salen de
 * pantalla
 */
void *player_bullet_manager_thread(void *arg) {
  (void)arg;
  while (game_running.load()) {
    {
      std::lock_guard<std::mutex> lock(bullet_mutex);
      for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].active) {
          bullets[i].y -= PLAYER_BULLET_SPEED;
          if (bullets[i].y < 1)
            bullets[i].active = false;
        }
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(UPDATE_INTERVAL_MS));
  }
  return nullptr;
}

/**
 * Hilo 2: Manejo de balas enemigas
 * Actualiza la posición de las balas enemigas y elimina las que salen de
 * pantalla
 */
void *enemy_bullet_manager_thread(void *arg) {
  (void)arg;
  while (game_running.load()) {
    for (int b = 0; b < MAX_BULLETS; b++) {
      if (ebullets[b].active) {
        ebullets[b].y += ENEMY_BULLET_SPEED;
        if (ebullets[b].y >= screen_h)
          ebullets[b].active = false;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(UPDATE_INTERVAL_MS));
  }
  return nullptr;
}

/**
 * Hilo 3: Detector de colisiones entre balas del jugador y enemigos
 * Detecta cuando las balas del jugador atinan
 */

void *player_bullet_collision_thread(void *arg) {
  (void)arg;
  while (game_running.load()) {
    {
      std::lock_guard<std::mutex> lock1(bullet_mutex);
      std::lock_guard<std::mutex> lock2(enemy_mutex);

      for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].active) {
          for (int e = 0; e < MAX_ENEMIES; e++) {
            if (enemies[e].alive) {
              int bx = static_cast<int>(std::round(bullets[i].x));
              int by = static_cast<int>(std::round(bullets[i].y));
              int ex = static_cast<int>(std::round(enemies[e].x));
              int ey = static_cast<int>(std::round(enemies[e].y));

              if (bx >= ex && bx < ex + ENEMY_W && by >= ey &&
                  by < ey + ENEMY_H) {
                enemies[e].alive = false;
                bullets[i].active = false;
                {
                  std::lock_guard<std::mutex> score_lock(score_mutex);
                  player_score += 10;
                }
              }
            }
          }
        }
      }
    }
    std::this_thread::sleep_for(
        std::chrono::milliseconds(UPDATE_INTERVAL_MS / 2));
  }
  return nullptr;
}

/**
 * Hilo 4: Controlador de movimiento de enemigos
 * Maneja el movimiento horizontal y vertical de los enemigos
 */
void *enemy_movement_controller_thread(void *arg) {
  (void)arg;
  int enemy_direction = 1;
  int tick_counter = 0;

  while (game_running.load()) {
    if (tick_counter % ENEMY_MOVEMENT_INTERVAL == 0) {
      std::lock_guard<std::mutex> lock(enemy_mutex);

      int wall_collision = 0;
      for (int e = 0; e < MAX_ENEMIES; e++) {
        if (enemies[e].alive) {
          int next_x = enemies[e].x + enemy_direction;
          if (next_x < 1 || next_x > screen_w - 2) {
            wall_collision = 1;
            break;
          }
        }
      }

      if (wall_collision) {
        enemy_direction = -enemy_direction;
        if (!enemy_stop_descent.load()) {
          float lowest_enemy_y = -1.0f;
          for (int e = 0; e < MAX_ENEMIES; e++) {
            if (enemies[e].alive && enemies[e].y > lowest_enemy_y)
              lowest_enemy_y = enemies[e].y;
          }

          if (lowest_enemy_y + ENEMY_H + ENEMY_H >
              static_cast<float>(MAX_ENEMY_Y)) {
            enemy_stop_descent.store(true);
          } else {
            for (int e = 0; e < MAX_ENEMIES; e++) {
              if (enemies[e].alive)
                enemies[e].y += ENEMY_H;
            }
          }
        }
      } else {
        for (int e = 0; e < MAX_ENEMIES; e++) {
          if (enemies[e].alive)
            enemies[e].x += enemy_direction;
        }
      }
    }

    tick_counter++;
    std::this_thread::sleep_for(std::chrono::milliseconds(UPDATE_INTERVAL_MS));
  }
  return nullptr;
}

/**
 * Hilo 5: Controlador de disparos enemigos
 * Maneja los disparos aleatorios por parte de los enemigos
 */
void *enemy_shooting_controller_thread(void *arg) {
  (void)arg;
  while (game_running.load()) {
    {
      std::lock_guard<std::mutex> lock(enemy_mutex);
      for (int e = 0; e < MAX_ENEMIES; e++) {
        if (enemies[e].alive) {
          if ((std::rand() % ENEMY_SHOOTING_DENOMINATOR) <
              ENEMY_SHOOTING_PROBABILITY) {
            for (int b = 0; b < MAX_BULLETS; b++) {
              if (!ebullets[b].active) {
                ebullets[b].active = true;
                ebullets[b].x = enemies[e].x + ENEMY_W / 2.0f;
                ebullets[b].y = enemies[e].y + ENEMY_H;
                break;
              }
            }
          }
        }
      }
    }
    std::this_thread::sleep_for(
        std::chrono::milliseconds(UPDATE_INTERVAL_MS * 2));
  }
  return nullptr;
}

/**
 * Hilo 6: Detector de colisiones entre balas enemigas y jugador
 * Detecta cuando las balas enemigas atinan al jugador y aplica efecto visual
 * de daño
 */
void *enemy_bullet_collision_thread(void *arg) {
  (void)arg;
  while (game_running.load()) {
    for (int b = 0; b < MAX_BULLETS; b++) {
      if (ebullets[b].active) {
        if (static_cast<int>(ebullets[b].y) >= ship_y) {
          int bullet_x = static_cast<int>(std::round(ebullets[b].x));
          int bullet_y = static_cast<int>(std::round(ebullets[b].y));
          int ship_left = static_cast<int>(ship_x - SHIP_W / 2.0f);
          int ship_right = ship_left + SHIP_W - 1;
          int ship_top = ship_y;
          int ship_bottom = ship_y + SHIP_H - 1;

          if (bullet_x >= ship_left && bullet_x <= ship_right &&
              bullet_y >= ship_top && bullet_y <= ship_bottom) {
            ebullets[b].active = false;
            {
              std::lock_guard<std::mutex> lock(game_state_mutex);
              player_lives -= 1;
              player_hit = true;
              damage_flash_start_ms = now_ms();
              if (player_lives <= 0)
                game_running = false;
            }
          } else if (bullet_y >= screen_h) {
            ebullets[b].active = false;
          }
        }
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(UPDATE_INTERVAL_MS));
  }
  return nullptr;
}

/**
 * Hilo 7: Verificador de completación
 * Verifica si todos los enemigos han sido eliminados para avanzar
 */
void *level_completion_checker_thread(void *arg) {
  (void)arg;
  while (game_running.load()) {
    {
      std::lock_guard<std::mutex> lock(enemy_mutex);
      int enemies_remaining = 0;
      for (int e = 0; e < MAX_ENEMIES; e++) {
        if (enemies[e].alive) {
          enemies_remaining = 1;
          break;
        }
      }

      if (!enemies_remaining) {
        {
          std::lock_guard<std::mutex> lock(game_state_mutex);
          current_level++;
          if (current_level > MAX_GAME_LEVELS) {
            game_completed = true;
            game_running = false;
          }
          cv_level_complete.notify_all();
        }
        reset_level();
      }
    }
    std::this_thread::sleep_for(
        std::chrono::milliseconds(UPDATE_INTERVAL_MS * 3));
  }
  return nullptr;
}

/**
 * Hilo 8: Monitor de estado del juego
 * Monitorea el estado general del juego y ve si le quedan vidas al jugador.
 */
void *game_state_monitor_thread(void *arg) {
  (void)arg;
  while (game_running.load()) {
    {
      std::unique_lock<std::mutex> lock(game_state_mutex);
      cv_game_state.wait_for(lock, std::chrono::milliseconds(100));

      if (player_lives <= 0) {
        game_running = false;
        cv_game_state.notify_all();
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  return nullptr;
}

/**
 * Hilo 10: Gestor de efectos visuales, el de daño
 */
void *visual_effects_manager_thread(void *arg) {
  (void)arg;
  while (game_running.load()) {
    if (player_hit.load()) {
      if (now_ms() - damage_flash_start_ms >= DAMAGE_FLASH_DURATION_MS) {
        player_hit = false;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(UPDATE_INTERVAL_MS));
  }
  return nullptr;
}

/**
 * Hilo 9: Gestor de puntuación
 * Maneja la puntuación y otorga vidas bonus
 */
void *score_manager_thread(void *arg) {
  (void)arg;
  int last_score = 0;
  while (game_running.load()) {
    {
      std::lock_guard<std::mutex> lock(score_mutex);
      if (player_score != last_score) {
        last_score = player_score;
        // Agregar vida bonus cada 300 puntos
        if (player_score > 0 && player_score % 300 == 0) {
          std::lock_guard<std::mutex> state_lock(game_state_mutex);
          if (player_lives < 5)
            player_lives++;
        }
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  return nullptr;
}

/**
 * Dibuja la pantalla del juego con todos los elementos
 */
void draw_screen() {
  // Capturar estado bajo lock
  struct GameSnapshot {
    int score;
    int lives;
    int level;
    int ship_x;
    int ship_y;
    bool is_hit;
    std::vector<std::pair<int, int>> player_bullets;
    std::vector<std::pair<int, int>> enemy_bullets;
    std::vector<std::pair<int, int>> alive_enemies;
  } snapshot;

  {
    std::lock_guard<std::mutex> lock(game_state_mutex);
    snapshot.score = player_score;
    snapshot.lives = player_lives;
    snapshot.level = current_level;
    snapshot.ship_x = ship_x;
    snapshot.ship_y = ship_y;

    snapshot.is_hit = player_hit.load();

    for (int i = 0; i < MAX_BULLETS; i++)
      if (bullets[i].active)
        snapshot.player_bullets.emplace_back(
            static_cast<int>(std::round(bullets[i].x)),
            static_cast<int>(std::round(bullets[i].y)));
    for (int i = 0; i < MAX_BULLETS; i++)
      if (ebullets[i].active)
        snapshot.enemy_bullets.emplace_back(
            static_cast<int>(std::round(ebullets[i].x)),
            static_cast<int>(std::round(ebullets[i].y)));
    for (int i = 0; i < MAX_ENEMIES; i++)
      if (enemies[i].alive)
        snapshot.alive_enemies.emplace_back(
            static_cast<int>(std::round(enemies[i].x)),
            static_cast<int>(std::round(enemies[i].y)));
  }

  // Dibujar en buffer trasero
  if (!backwin) {
    clear();
    int hud_left = 2;
    int hud_best = screen_w / 2 - 12;
    int hud_lives = screen_w / 2 + 6;
    int hud_level = screen_w - 12;
    if (hud_best < hud_left + 12)
      hud_best = hud_left + 12;
    if (hud_lives <= hud_best + 10)
      hud_lives = hud_best + 12;
    if (hud_level <= hud_lives + 8)
      hud_level = hud_lives + 10;
    if (hud_level >= screen_w - 1)
      hud_level = std::max(hud_lives + 6, screen_w - 12);
    mvprintw(0, hud_left, "Puntaje: %d", snapshot.score);
    mvprintw(0, hud_best, "Mejor: %d", saved_highscore);
    mvprintw(0, hud_lives, "Vidas: %d", snapshot.lives);
    mvprintw(0, hud_level, "Nivel: %d", snapshot.level);
    int sx = snapshot.ship_x - SHIP_W / 2;
    if (sx < 0)
      sx = 0;
    if (sx + SHIP_W >= screen_w)
      sx = screen_w - SHIP_W;

    // Aplicar efecto visual de daño (parpadeo rojo)
    if (snapshot.is_hit && has_colors())
      attron(COLOR_PAIR(2));
    else if (has_colors())
      attron(COLOR_PAIR(1));

    for (int r = 0; r < SHIP_H; r++) {
      mvaddnstr(snapshot.ship_y + r, sx, SHIP_ART[r], SHIP_W);
    }
    if (has_colors())
      attroff(COLOR_PAIR(1) | COLOR_PAIR(2));

    if (has_colors())
      attron(COLOR_PAIR(3));
    for (auto &p : snapshot.player_bullets)
      mvaddch(p.second, p.first, '|');
    for (auto &p : snapshot.enemy_bullets)
      mvaddch(p.second, p.first, '!');
    if (has_colors())
      attroff(COLOR_PAIR(3));
    for (auto &en : snapshot.alive_enemies) {
      int ex = en.first - 1;
      int ey = en.second;
      if (ex < 0)
        ex = 0;
      if (ex + ENEMY_W >= screen_w)
        ex = screen_w - ENEMY_W;
      if (has_colors())
        attron(COLOR_PAIR(2));
      const char **art = enemy_art_for_level(snapshot.level);
      for (int r = 0; r < ENEMY_H; r++) {
        mvaddnstr(ey + r, ex, art[r], ENEMY_W);
      }
      if (has_colors())
        attroff(COLOR_PAIR(2));
    }
    refresh();
    return;
  }

  werase(backwin);
  // Calcular posiciones del HUD
  int bhud_left = 2;
  int bhud_best = screen_w / 2 - 12;
  int bhud_lives = screen_w / 2 + 6;
  int bhud_level = screen_w - 12;
  if (bhud_best < bhud_left + 12)
    bhud_best = bhud_left + 12;
  if (bhud_lives <= bhud_best + 10)
    bhud_lives = bhud_best + 12;
  if (bhud_level <= bhud_lives + 8)
    bhud_level = bhud_lives + 10;
  if (bhud_level >= screen_w - 1)
    bhud_level = std::max(bhud_lives + 6, screen_w - 12);
  mvwprintw(backwin, 0, bhud_left, "Puntaje: %d", snapshot.score);
  mvwprintw(backwin, 0, bhud_best, "Mejor: %d", saved_highscore);
  mvwprintw(backwin, 0, bhud_lives, "Vidas: %d", snapshot.lives);
  mvwprintw(backwin, 0, bhud_level, "Nivel: %d", snapshot.level);

  // Centrar nave
  int ship_screen_x = snapshot.ship_x - SHIP_W / 2;
  if (ship_screen_x < 0)
    ship_screen_x = 0;
  if (ship_screen_x + SHIP_W >= screen_w)
    ship_screen_x = screen_w - SHIP_W;

  // Aplicar efecto visual de daño (parpadeo rojo)
  if (snapshot.is_hit && has_colors())
    wattron(backwin, COLOR_PAIR(2));
  else if (has_colors())
    wattron(backwin, COLOR_PAIR(1));

  for (int r = 0; r < SHIP_H; r++) {
    mvwaddnstr(backwin, snapshot.ship_y + r, ship_screen_x, SHIP_ART[r],
               SHIP_W);
  }
  if (has_colors())
    wattroff(backwin, COLOR_PAIR(1) | COLOR_PAIR(2));

  if (has_colors())
    wattron(backwin, COLOR_PAIR(3));
  for (auto &p : snapshot.player_bullets)
    mvwaddch(backwin, p.second, p.first, '|');
  for (auto &p : snapshot.enemy_bullets)
    mvwaddch(backwin, p.second, p.first, '!');
  if (has_colors())
    wattroff(backwin, COLOR_PAIR(3));

  for (auto &en : snapshot.alive_enemies) {
    int ex = en.first - 1;
    int ey = en.second;
    if (ex < 0)
      ex = 0;
    if (ex + ENEMY_W >= screen_w)
      ex = screen_w - ENEMY_W;
    if (has_colors())
      wattron(backwin, COLOR_PAIR(2));
    const char **art = enemy_art_for_level(snapshot.level);
    for (int r = 0; r < ENEMY_H; r++) {
      mvwaddnstr(backwin, ey + r, ex, art[r], ENEMY_W);
    }
    if (has_colors())
      wattroff(backwin, COLOR_PAIR(2));
  }

  // Pasar del buffer fuera de pantalla a la pantalla real
  wnoutrefresh(backwin);
  doupdate();
}

/**
 * Bucle de entrada de teclado
 * Maneja el input del usuario
 */
void input_loop() {
  int ch;
  nodelay(stdscr, TRUE);
  keypad(stdscr, TRUE);
  while (game_running.load()) {
    ch = getch();
    if (ch == ERR) {
      int hk = held_key.load();
      if (hk != 0) {
        long long elapsed = now_ms() - last_move_ms.load();
        if (elapsed > MOVEMENT_TIMEOUT_MS) {
          held_key.store(0);
          move_left = false;
          move_right = false;
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(INPUT_INTERVAL_MS));
      continue;
    }

    // Terminar juego
    if (ch == 'q' || ch == 'Q') {
      game_running = false;
    } else if (ch == KEY_LEFT || ch == 'a' || ch == 'A') {
      // tecla izquierda
      held_key.store(-1);
      move_left = true;
      move_right = false;
      last_move_ms.store(now_ms());
    } else if (ch == KEY_RIGHT || ch == 'd' || ch == 'D') {
      // tecla derecha
      held_key.store(1);
      move_right = true;
      move_left = false;
      last_move_ms.store(now_ms());
    } else if (ch == ' ' || ch == 'k' || ch == 'K') {
      want_fire = true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(INPUT_INTERVAL_MS));
  }
}

/**
 * Bucle de actualización principal del juego
 * Maneja el movimiento del jugador y disparos
 */
void update_loop() {
  while (game_running.load()) {
    {
      std::lock_guard<std::mutex> lock(game_state_mutex);
      // Flag de movimiento
      if (move_left.load()) {
        ship_fx = std::max(1.0f, ship_fx - PLAYER_MOVEMENT_SPEED);
        ship_x = static_cast<int>(std::round(ship_fx));
      }
      if (move_right.load()) {
        ship_fx = std::min(static_cast<float>(screen_w - 2),
                           ship_fx + PLAYER_MOVEMENT_SPEED);
        ship_x = static_cast<int>(std::round(ship_fx));
      }
      // Solicitud de disparo
      if (want_fire.load()) {
        std::lock_guard<std::mutex> bullet_lock(bullet_mutex);
        for (int i = 0; i < MAX_BULLETS; i++)
          if (!bullets[i].active) {
            bullets[i].active = true;
            bullets[i].x = ship_fx;
            bullets[i].y = ship_y - 1;
            break;
          }
        want_fire = false;
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(UPDATE_INTERVAL_MS));
  }
}

// Muestra la pantalla de fin de juego, true si el jugador quiere reiniciar,
// false si quiere salir

bool show_gameover() {
  nodelay(stdscr, FALSE);
  clear();
  int bw = std::min(60, screen_w - 4);
  int bx = (screen_w - bw) / 2;
  int by = std::max(2, screen_h / 2 - 8);
  mvhline(by, bx, '=', bw);
  attron(A_BOLD);
  mvprintw(by + 1, bx + (bw / 2) - 5, "GAME OVER");
  attroff(A_BOLD);
  mvhline(by + 3, bx, '=', bw);

  // Estadisticas
  mvprintw(by + 5, bx + 4, "Nivel alcanzado: %d/%d", current_level,
           MAX_GAME_LEVELS);
  mvprintw(by + 6, bx + 4, "Puntaje final: %d", player_score);
  mvprintw(by + 7, bx + 4, "Enemigos eliminados: %d", (player_score / 10));

  auto hs = load_highscores();
  mvprintw(by + 9, bx + 4, "Puntuaciones mas altas:");
  for (int i = 0; i < 3; i++) {
    mvprintw(by + 11 + i, bx + 6, "%d. %d", i + 1, hs[i]);
  }

  mvprintw(by + 15, bx + 4, "Presiona 'r' para reiniciar o 'q' para salir");
  refresh();
  int ch;
  while (true) {
    ch = getch();
    if (ch == 'r' || ch == 'R') {
      return true;
    } else if (ch == 'q' || ch == 'Q') {
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
}

/**
 * Muestra la pantalla de victoria
 */
bool show_victory_screen() {
  nodelay(stdscr, FALSE);
  clear();

  int bw = std::min(70, screen_w - 4);
  int bx = (screen_w - bw) / 2;
  int by = std::max(2, screen_h / 2 - 10);

  mvhline(by, bx, '=', bw);
  mvhline(by + 2, bx, '-', bw);

  attron(A_BOLD | A_BLINK);
  mvprintw(by + 1, bx + (bw / 2) - 10, "*** HAS GANADO! ***");
  attroff(A_BOLD | A_BLINK);

  attron(A_BOLD);
  mvprintw(by + 4, bx + (bw / 2) - 15, "FELICITACIONES COMANDANTE!");
  mvprintw(by + 5, bx + (bw / 2) - 12, "Has derrotado a todos los enemigos");
  attroff(A_BOLD);

  mvhline(by + 7, bx, '-', bw);

  // Estadísticas
  mvprintw(by + 9, bx + 4, "Niveles completados: %d/%d", MAX_GAME_LEVELS,
           MAX_GAME_LEVELS);
  mvprintw(by + 10, bx + 4, "Puntaje final: %d", player_score);

  auto hs = load_highscores();
  mvprintw(by + 12, bx + 4, "Mejores puntuaciones:");
  for (int i = 0; i < 3; i++) {
    mvprintw(by + 14 + i, bx + 6, "%d. %d", i + 1, hs[i]);
  }

  mvhline(by + 18, bx, '=', bw);
  mvprintw(by + 20, bx + 4,
           "Presiona 'r' para jugar de nuevo o 'q' para salir");

  refresh();
  int ch;
  while (true) {
    ch = getch();
    if (ch == 'r' || ch == 'R') {
      return true;
    } else if (ch == 'q' || ch == 'Q') {
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
}

enum MenuChoice {
  MENU_PLAY = 0,
  MENU_INSTR = 1,
  MENU_SCORES = 2,
  MENU_QUIT = 3
};

int show_menu() {
  nodelay(stdscr, FALSE);
  keypad(stdscr, TRUE);
  int choice = 0;
  const char *items[] = {"Jugar", "Instrucciones", "Puntajes", "Salir"};
  int nitems = 4;
  while (true) {
    // Calcula el tamaño de pantalla
    getmaxyx(stdscr, screen_h, screen_w);
    clear();
    int box_w = std::min(60, screen_w - 4);
    int box_x = std::max(2, (screen_w - box_w) / 2);
    int y0 = 3;
    mvhline(y0, box_x, '-', box_w);
    const char *title = "GALAGA";
    const char *subtitle = "Un juego de consola";
    int title_x = box_x + (box_w - static_cast<int>(std::strlen(title))) / 2;
    int subtitle_x =
        box_x + (box_w - static_cast<int>(std::strlen(subtitle))) / 2;
    title_x =
        std::max(box_x + 1, std::min(title_x, box_x + box_w -
                                                  (int)std::strlen(title) - 1));
    subtitle_x = std::max(
        box_x + 1,
        std::min(subtitle_x, box_x + box_w - (int)std::strlen(subtitle) - 1));
    attron(A_BOLD);
    mvprintw(y0 + 1, title_x, "%s", title);
    mvprintw(y0 + 2, subtitle_x, "%s", subtitle);
    attroff(A_BOLD);
    mvhline(y0 + 6, box_x, '-', box_w);

    int start_y = y0 + 8;
    int midx = screen_w / 2;
    for (int i = 0; i < nitems; i++) {
      int y = start_y + i * 2;
      int x = midx - 6;
      if (i == choice) {
        attron(A_REVERSE | A_BOLD);
        mvprintw(y, x, "%s", items[i]);
        attroff(A_REVERSE | A_BOLD);
      } else {
        mvprintw(y, x, "%s", items[i]);
      }
    }

    mvprintw(screen_h - 5, box_x + 2, "CONTROLES:");
    mvprintw(screen_h - 4, box_x + 4,
             "A/Izquierda - Mover izquierda    D/Derecha - Mover derecha");
    mvprintw(screen_h - 3, box_x + 4,
             "ESPACIO/K - Disparar             Q - Salir del juego");
    mvprintw(screen_h - 2, box_x + 2,
             "Usa flechas para navegar menu. Enter para seleccionar.");
    refresh();

    int ch = getch();
    if (ch == KEY_UP || ch == 'w' || ch == 'W') {
      choice = (choice - 1 + nitems) % nitems;
    } else if (ch == KEY_DOWN || ch == 's' || ch == 'S') {
      choice = (choice + 1) % nitems;
    } else if (ch == 10 || ch == KEY_ENTER) {
      return choice;
    } else if (ch == 'q' || ch == 'Q') {
      return MENU_QUIT;
    }
  }
}

void show_highscores() {
  nodelay(stdscr, FALSE);
  clear();
  auto hs = load_highscores();
  mvprintw(3, 4, "PUNTUACIONES MAS ALTAS");
  for (int i = 0; i < 3; i++) {
    mvprintw(6 + i * 2, 8, "%d. %d", i + 1, hs[i]);
  }
  mvprintw(screen_h - 2, 4, "Presiona cualquier tecla para volver al menu.");
  refresh();
  getch();
}

void show_instructions() {
  nodelay(stdscr, FALSE);
  clear();
  mvprintw(2, 4, "Instrucciones:");
  mvprintw(4, 4, "A / Left  - Mover a la izquierda");
  mvprintw(5, 4, "D / Right - Mover a la derecha");
  mvprintw(6, 4, "Space / K - Disparar");
  mvprintw(
      8, 4,
      "Objetivo: destruir a todos los enemigos sin perder todas tus vidas.");
  mvprintw(
      10, 4,
      "Las naves enemigas pueden disparar. Evita sus balas o recibirás daño.");
  mvprintw(12, 4, "Presiona cualquier tecla para volver al menú.");
  refresh();
  getch();
}

int main() {
  initscr();
  cbreak();
  noecho();
  curs_set(0);
  if (has_colors()) {
    start_color();
    use_default_colors();
    init_pair(1, COLOR_GREEN, -1);  // Jugador
    init_pair(2, COLOR_RED, -1);    // Enemigo
    init_pair(3, COLOR_YELLOW, -1); // Balas
  }
  getmaxyx(stdscr, screen_h, screen_w);

  auto hs_init = load_highscores();
  saved_highscore = hs_init.empty() ? 0 : hs_init[0];

  bool running_app = true;
  while (running_app) {
    int choice = show_menu();
    clear();
    refresh();
    if (choice == MENU_QUIT)
      break;
    if (choice == MENU_INSTR) {
      show_instructions();
      continue;
    }
    if (choice == MENU_SCORES) {
      show_highscores();
      continue;
    }

    init_game();
    reset_level();

    // Crear todos los hilos del juego
    std::thread t_input, t_update;
    std::thread t_player_bullet_mgr, t_enemy_bullet_mgr,
        t_player_bullet_collision;
    std::thread t_enemy_move, t_enemy_shoot;
    std::thread t_enemy_bullet_collision, t_level_checker;
    std::thread t_game_monitor, t_score_mgr, t_visual_effects;

    game_running = true;

    // Iniciar todos los hilos
    t_input = std::thread(input_loop);
    t_update = std::thread(update_loop);
    t_player_bullet_mgr = std::thread(player_bullet_manager_thread, nullptr);
    t_enemy_bullet_mgr = std::thread(enemy_bullet_manager_thread, nullptr);
    t_player_bullet_collision =
        std::thread(player_bullet_collision_thread, nullptr);
    t_enemy_move = std::thread(enemy_movement_controller_thread, nullptr);
    t_enemy_shoot = std::thread(enemy_shooting_controller_thread, nullptr);
    t_enemy_bullet_collision =
        std::thread(enemy_bullet_collision_thread, nullptr);
    t_level_checker = std::thread(level_completion_checker_thread, nullptr);
    t_game_monitor = std::thread(game_state_monitor_thread, nullptr);
    t_score_mgr = std::thread(score_manager_thread, nullptr);
    t_visual_effects = std::thread(visual_effects_manager_thread, nullptr);

    // Bucle del juego
    while (true) {
      while (game_running.load()) {
        draw_screen();
        std::this_thread::sleep_for(
            std::chrono::milliseconds(RENDER_INTERVAL_MS));
      }

      // Unir todos los hilos
      if (t_input.joinable())
        t_input.join();
      if (t_update.joinable())
        t_update.join();
      if (t_player_bullet_mgr.joinable())
        t_player_bullet_mgr.join();
      if (t_enemy_bullet_mgr.joinable())
        t_enemy_bullet_mgr.join();
      if (t_player_bullet_collision.joinable())
        t_player_bullet_collision.join();
      if (t_enemy_move.joinable())
        t_enemy_move.join();
      if (t_enemy_shoot.joinable())
        t_enemy_shoot.join();
      if (t_enemy_bullet_collision.joinable())
        t_enemy_bullet_collision.join();
      if (t_level_checker.joinable())
        t_level_checker.join();
      if (t_game_monitor.joinable())
        t_game_monitor.join();
      if (t_score_mgr.joinable())
        t_score_mgr.join();
      if (t_visual_effects.joinable())
        t_visual_effects.join();

      draw_screen();
      update_highscores_if_needed(player_score);
      {
        auto tmp = load_highscores();
        saved_highscore = tmp.empty() ? 0 : tmp[0];
      }

      bool restart;
      if (game_completed) {
        restart = show_victory_screen();
      } else {
        restart = show_gameover();
      }

      if (!restart)
        break;

      // Reiniciar el juego
      init_game();
      reset_level();
      game_running = true;

      // Reiniciar todos los hilos
      t_input = std::thread(input_loop);
      t_update = std::thread(update_loop);
      t_player_bullet_mgr = std::thread(player_bullet_manager_thread, nullptr);
      t_enemy_bullet_mgr = std::thread(enemy_bullet_manager_thread, nullptr);
      t_player_bullet_collision =
          std::thread(player_bullet_collision_thread, nullptr);
      t_enemy_move = std::thread(enemy_movement_controller_thread, nullptr);
      t_enemy_shoot = std::thread(enemy_shooting_controller_thread, nullptr);
      t_enemy_bullet_collision =
          std::thread(enemy_bullet_collision_thread, nullptr);
      t_level_checker = std::thread(level_completion_checker_thread, nullptr);
      t_game_monitor = std::thread(game_state_monitor_thread, nullptr);
      t_score_mgr = std::thread(score_manager_thread, nullptr);
      t_visual_effects = std::thread(visual_effects_manager_thread, nullptr);
    }

    // Finalizar
    if (t_input.joinable())
      t_input.join();
    if (t_update.joinable())
      t_update.join();
    if (t_player_bullet_mgr.joinable())
      t_player_bullet_mgr.join();
    if (t_enemy_bullet_mgr.joinable())
      t_enemy_bullet_mgr.join();
    if (t_player_bullet_collision.joinable())
      t_player_bullet_collision.join();
    if (t_enemy_move.joinable())
      t_enemy_move.join();
    if (t_enemy_shoot.joinable())
      t_enemy_shoot.join();
    if (t_enemy_bullet_collision.joinable())
      t_enemy_bullet_collision.join();
    if (t_level_checker.joinable())
      t_level_checker.join();
    if (t_game_monitor.joinable())
      t_game_monitor.join();
    if (t_score_mgr.joinable())
      t_score_mgr.join();
    if (t_visual_effects.joinable())
      t_visual_effects.join();
  }

  endwin();
  return 0;
}
