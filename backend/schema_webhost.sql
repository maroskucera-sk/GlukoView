-- =============================================================================
-- GlukoView Insulin Log — tabuľky pre shared hosting (Websupport a pod.)
-- =============================================================================
-- DÔLEŽITÉ: databázu vytvorte v paneli hostingu (WebAdmin).
-- Tento súbor NEOBSAHUJE CREATE DATABASE — importujte ho do už existujúcej DB.
-- Charset DB nastavte na utf8mb4.
-- =============================================================================

SET NAMES utf8mb4;
SET time_zone = '+00:00';

CREATE TABLE IF NOT EXISTS families (
  id            BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  name          VARCHAR(120)    NOT NULL,
  access_key_hash VARCHAR(255)  NOT NULL,
  created_at    DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at    DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3)
                  ON UPDATE CURRENT_TIMESTAMP(3),
  PRIMARY KEY (id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS profiles (
  id            BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  family_id     BIGINT UNSIGNED NOT NULL,
  name          VARCHAR(80)     NOT NULL,
  color_hex     CHAR(7)         NOT NULL DEFAULT '#4CAF50',
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
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS insulins (
  id              BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  family_id       BIGINT UNSIGNED NOT NULL,
  name            VARCHAR(80)     NOT NULL,
  category        ENUM('bolus', 'basal') NOT NULL,
  color_hex       CHAR(7)         NOT NULL DEFAULT '#2196F3',
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
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS scenes (
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
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS injection_sites (
  id            BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  family_id     BIGINT UNSIGNED NOT NULL,
  name          VARCHAR(80)     NOT NULL,
  sort_order    SMALLINT        NOT NULL DEFAULT 0,
  is_archived   TINYINT(1)      NOT NULL DEFAULT 0,
  created_at    DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at    DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3)
                  ON UPDATE CURRENT_TIMESTAMP(3),
  PRIMARY KEY (id),
  KEY idx_injection_sites_family (family_id, is_archived, sort_order),
  CONSTRAINT fk_injection_sites_family
    FOREIGN KEY (family_id) REFERENCES families (id)
    ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS family_settings (
  family_id              BIGINT UNSIGNED NOT NULL,
  nightscout_url         VARCHAR(512)    NULL,
  nightscout_token       VARCHAR(255)    NULL,
  basal_reminder_time    CHAR(5)         NULL,
  glucose_unit           ENUM('mmol', 'mgdl') NOT NULL DEFAULT 'mmol',
  duplicate_bolus_minutes SMALLINT UNSIGNED NOT NULL DEFAULT 20,
  current_injection_site VARCHAR(80)     NULL,
  updated_at             DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3)
                           ON UPDATE CURRENT_TIMESTAMP(3),
  PRIMARY KEY (family_id),
  CONSTRAINT fk_settings_family
    FOREIGN KEY (family_id) REFERENCES families (id)
    ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS dose_records (
  id                BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
  family_id         BIGINT UNSIGNED NOT NULL,
  uuid              CHAR(36)        NOT NULL,
  profile_id        BIGINT UNSIGNED NOT NULL,
  insulin_id        BIGINT UNSIGNED NOT NULL,
  scene_id          BIGINT UNSIGNED NULL,
  units             DECIMAL(6,2)    NOT NULL,
  administered_at   DATETIME(3)     NOT NULL,
  glucose_value     DECIMAL(6,2)    NULL,
  glucose_unit      ENUM('mmol', 'mgdl') NULL,
  glucose_trend     VARCHAR(16)     NULL,
  carbs_sj          DECIMAL(5,1)    NULL,
  carbs_grams       DECIMAL(6,1)    NULL,
  injection_site    VARCHAR(80)     NULL,
  note              VARCHAR(280)    NULL,
  created_at        DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
  updated_at        DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3)
                      ON UPDATE CURRENT_TIMESTAMP(3),
  deleted_at        DATETIME(3)     NULL,
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
    ON DELETE SET NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
