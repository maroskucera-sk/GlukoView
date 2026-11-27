-- =============================================================================
-- GlukoView Insulin Log — MySQL 8+ schéma
-- Charset: utf8mb4 (slovenčina, emoji ikony)
-- =============================================================================

SET NAMES utf8mb4;
SET time_zone = '+00:00';

CREATE DATABASE IF NOT EXISTS glukoview_insulin
  CHARACTER SET utf8mb4
  COLLATE utf8mb4_unicode_ci;

USE glukoview_insulin;

-- -----------------------------------------------------------------------------
-- Rodinné konto (jeden prístupový kľúč pre celú rodinu)
-- -----------------------------------------------------------------------------
CREATE TABLE families (
  id            BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  name          VARCHAR(120)    NOT NULL,
  -- Ukladá sa len hash (password_hash / Argon2id). Plaintext kľúč nikdy do DB.
  access_key_hash VARCHAR(255)  NOT NULL,
  created_at    DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at    DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3)
                  ON UPDATE CURRENT_TIMESTAMP(3),
  PRIMARY KEY (id)
) ENGINE=InnoDB;

-- -----------------------------------------------------------------------------
-- Profily členov rodiny (Maroš, Veronika, Melinka, …)
-- Výber profilu je na zariadení; server len eviduje kto podal dávku.
-- -----------------------------------------------------------------------------
CREATE TABLE profiles (
  id            BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  family_id     BIGINT UNSIGNED NOT NULL,
  name          VARCHAR(80)     NOT NULL,
  color_hex     CHAR(7)         NOT NULL DEFAULT '#4CAF50',  -- #RRGGBB
  sort_order    SMALLINT        NOT NULL DEFAULT 0,
  is_active     TINYINT(1)      NOT NULL DEFAULT 1,
  created_at    DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at    DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3)
                  ON UPDATE CURRENT_TIMESTAMP(3),
  PRIMARY KEY (id),
  KEY idx_profiles_family (family_id, is_active, sort_order),
  CONSTRAINT fk_profiles_family
    FOREIGN KEY (family_id) REFERENCES families (id)
    ON DELETE CASCADE
) ENGINE=InnoDB;

-- -----------------------------------------------------------------------------
-- Typy inzulínov (dynamický číselník, zdieľaný v rodine)
-- category: bolus = rýchlo pôsobiaci, basal = bazálny
-- -----------------------------------------------------------------------------
CREATE TABLE insulins (
  id              BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  family_id       BIGINT UNSIGNED NOT NULL,
  name            VARCHAR(80)     NOT NULL,
  category        ENUM('bolus', 'basal') NOT NULL,
  color_hex       CHAR(7)         NOT NULL DEFAULT '#2196F3',
  -- Dĺžka pôsobenia pre orientačný IOB (typicky 3.00–4.00 h pre bolus)
  duration_hours  DECIMAL(4,2)    NOT NULL DEFAULT 4.00,
  sort_order      SMALLINT        NOT NULL DEFAULT 0,
  is_archived     TINYINT(1)      NOT NULL DEFAULT 0,
  created_at      DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at      DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3)
                    ON UPDATE CURRENT_TIMESTAMP(3),
  PRIMARY KEY (id),
  KEY idx_insulins_family (family_id, is_archived, sort_order),
  CONSTRAINT fk_insulins_family
    FOREIGN KEY (family_id) REFERENCES families (id)
    ON DELETE CASCADE
) ENGINE=InnoDB;

-- -----------------------------------------------------------------------------
-- Scény / kontext (Raňajky, Obed, Korekcia, …)
-- icon: stabilný kľúč pre Material ikonu v aplikácii (napr. breakfast, dinner)
-- -----------------------------------------------------------------------------
CREATE TABLE scenes (
  id            BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  family_id     BIGINT UNSIGNED NOT NULL,
  name          VARCHAR(80)     NOT NULL,
  icon_key      VARCHAR(40)     NOT NULL DEFAULT 'meal',
  sort_order    SMALLINT        NOT NULL DEFAULT 0,
  is_archived   TINYINT(1)      NOT NULL DEFAULT 0,
  created_at    DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at    DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3)
                  ON UPDATE CURRENT_TIMESTAMP(3),
  PRIMARY KEY (id),
  KEY idx_scenes_family (family_id, is_archived, sort_order),
  CONSTRAINT fk_scenes_family
    FOREIGN KEY (family_id) REFERENCES families (id)
    ON DELETE CASCADE
) ENGINE=InnoDB;

-- -----------------------------------------------------------------------------
-- Rodinné nastavenia (1:1) — Nightscout, pripomienka bazálu
-- -----------------------------------------------------------------------------
CREATE TABLE family_settings (
  family_id              BIGINT UNSIGNED NOT NULL,
  nightscout_url         VARCHAR(512)    NULL,
  -- API Secret alebo Token; na hostingu držať HTTPS a DB mimo webroot
  nightscout_token       VARCHAR(255)    NULL,
  -- Lokálny čas rodiny „HH:MM“ (napr. 21:30); NULL = pripomienka vypnutá
  basal_reminder_time    CHAR(5)         NULL,
  -- Jednotky glykémie v UI: mmol | mgdl (Nightscout entries sú typicky mg/dL)
  glucose_unit           ENUM('mmol', 'mgdl') NOT NULL DEFAULT 'mmol',
  -- Prah varovania pred duplicitným bolusom (minúty)
  duplicate_bolus_minutes SMALLINT UNSIGNED NOT NULL DEFAULT 20,
  updated_at             DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3)
                           ON UPDATE CURRENT_TIMESTAMP(3),
  PRIMARY KEY (family_id),
  CONSTRAINT fk_settings_family
    FOREIGN KEY (family_id) REFERENCES families (id)
    ON DELETE CASCADE
) ENGINE=InnoDB;

-- -----------------------------------------------------------------------------
-- Záznamy podaného inzulínu
-- uuid: klientský identifikátor pre offline sync (idempotentný upsert)
-- -----------------------------------------------------------------------------
CREATE TABLE dose_records (
  id                BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  family_id         BIGINT UNSIGNED NOT NULL,
  uuid              CHAR(36)        NOT NULL,
  profile_id        BIGINT UNSIGNED NOT NULL,   -- kto podal
  insulin_id        BIGINT UNSIGNED NOT NULL,
  scene_id          BIGINT UNSIGNED NULL,
  units             DECIMAL(6,2)    NOT NULL,   -- napr. 0.50, 12.00
  administered_at   DATETIME(3)     NOT NULL,   -- čas podania (UTC)
  -- Glykémia pri podaní (voliteľné; predvyplnené z Nightscoutu)
  glucose_value     DECIMAL(6,2)    NULL,       -- v jednotkách glucose_unit rodiny
  glucose_unit      ENUM('mmol', 'mgdl') NULL,
  glucose_trend     VARCHAR(16)     NULL,       -- DoubleUp, SingleUp, FortyFiveUp, Flat, …
  -- Sacharidy (voliteľné) — SJ alebo gramy (alebo oboje)
  carbs_sj          DECIMAL(5,1)    NULL,
  carbs_grams       DECIMAL(6,1)    NULL,
  injection_site    ENUM('abdomen', 'thigh', 'arm', 'buttock') NULL,
  note              VARCHAR(280)    NULL,
  created_at        DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at        DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3)
                      ON UPDATE CURRENT_TIMESTAMP(3),
  deleted_at        DATETIME(3)     NULL,       -- soft delete pre sync
  PRIMARY KEY (id),
  UNIQUE KEY uq_dose_family_uuid (family_id, uuid),
  KEY idx_doses_family_time (family_id, administered_at),
  KEY idx_doses_family_updated (family_id, updated_at),
  KEY idx_doses_profile (profile_id),
  KEY idx_doses_insulin (insulin_id),
  CONSTRAINT fk_doses_family
    FOREIGN KEY (family_id) REFERENCES families (id)
    ON DELETE CASCADE,
  CONSTRAINT fk_doses_profile
    FOREIGN KEY (profile_id) REFERENCES profiles (id)
    ON DELETE RESTRICT,
  CONSTRAINT fk_doses_insulin
    FOREIGN KEY (insulin_id) REFERENCES insulins (id)
    ON DELETE RESTRICT,
  CONSTRAINT fk_doses_scene
    FOREIGN KEY (scene_id) REFERENCES scenes (id)
    ON DELETE SET NULL,
  CONSTRAINT chk_doses_units_positive
    CHECK (units > 0)
) ENGINE=InnoDB;

-- =============================================================================
-- Seed: ukážková rodina (NA PRODUKCII vygenerovať vlastný kľúč!)
-- Prístupový kľúč plaintext (len pre vývoj):  change-me-family-key
-- Hash nižšie je bcrypt za „change-me-family-key“ — pri deployi nahraďte.
-- =============================================================================

INSERT INTO families (id, name, access_key_hash) VALUES
  (1, 'Demo rodina', '$2y$10$Z1UyrFQLpT8X6rq6HqJN9esTiIhZ8ruM7MH0DbAKiraIr9GT2Vd8G');
-- Demo plaintext kľúč (LEN VÝVOJ): change-me-family-key
-- Na produkcii nahraďte vlastným kľúčom (krok 2: inštalačný skript / password_hash).

INSERT INTO profiles (family_id, name, color_hex, sort_order) VALUES
  (1, 'Maroš',     '#1976D2', 1),
  (1, 'Veronika',  '#C2185B', 2),
  (1, 'Melinka',   '#7B1FA2', 3);

INSERT INTO insulins (family_id, name, category, color_hex, duration_hours, sort_order) VALUES
  (1, 'NovoRapid',  'bolus', '#E53935', 5.00, 1),
  (1, 'Fiasp',      'bolus', '#FB8C00', 4.50, 2),
  (1, 'Tresiba',    'basal', '#1E88E5', 42.00, 3),
  (1, 'Lantus',     'basal', '#43A047', 24.00, 4);

INSERT INTO scenes (family_id, name, icon_key, sort_order) VALUES
  (1, 'Raňajky',     'breakfast', 1),
  (1, 'Desiata',     'snack',     2),
  (1, 'Obed',        'lunch',     3),
  (1, 'Olovrant',    'snack',     4),
  (1, 'Večera',      'dinner',    5),
  (1, 'Korekcia',    'correction',6),
  (1, 'Pred spaním', 'bedtime',   7);

INSERT INTO family_settings (
  family_id,
  nightscout_url,
  nightscout_token,
  basal_reminder_time,
  glucose_unit,
  duplicate_bolus_minutes
) VALUES (
  1,
  NULL,
  NULL,
  '21:30',
  'mmol',
  20
);
