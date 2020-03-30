BEGIN;

CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

CREATE SCHEMA IF NOT EXISTS "mzbh";
SET search_path TO 'mzbh';

CREATE OR REPLACE FUNCTION mzbh.trigger_set_timestamp()
RETURNS TRIGGER AS $$
BEGIN
      NEW.updated_at = NOW();
      RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- User
CREATE TABLE IF NOT EXISTS mzbh."user" (
    id uuid DEFAULT public.uuid_generate_v4() NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    email_address TEXT NOT NULL,

    password TEXT NOT NULL, -- scrypt hash with salt
    CONSTRAINT "user_pkey" PRIMARY KEY ("id") NOT DEFERRABLE INITIALLY IMMEDIATE
);

DROP TRIGGER IF EXISTS set_updated_at ON "mzbh"."user";
CREATE TRIGGER set_updated_at
    BEFORE UPDATE ON "mzbh"."user"
    FOR EACH ROW EXECUTE PROCEDURE trigger_set_timestamp();

-- Threads
CREATE TABLE IF NOT EXISTS mzbh.thread (
    id uuid DEFAULT public.uuid_generate_v4() NOT NULL,
    oleg_key TEXT UNIQUE,
    board TEXT,
    subject TEXT,
    created_at TIMESTAMPTZ DEFAULT now(),
    updated_at TIMESTAMPTZ DEFAULT now(),
    CONSTRAINT "thread_pkey" PRIMARY KEY ("id") NOT DEFERRABLE INITIALLY IMMEDIATE
);
CREATE INDEX threads_oleg_key ON mzbh.thread (oleg_key);

DROP TRIGGER IF EXISTS set_updated_at ON "mzbh"."thread";
CREATE TRIGGER set_updated_at
    BEFORE UPDATE ON "mzbh"."thread"
    FOR EACH ROW EXECUTE PROCEDURE trigger_set_timestamp();

-- Posts
CREATE TABLE mzbh.post (
    id uuid DEFAULT public.uuid_generate_v4() NOT NULL,
    oleg_key TEXT UNIQUE,
    fourchan_post_id BIGINT, -- The date of the post (unsignedix timestamp)
    fourchan_post_no BIGINT, -- The actual post ID on 4chan
    thread_uuid uuid REFERENCES mzbh.thread,
    board TEXT,
    body_content TEXT,
    replied_to_keys JSONB, -- JSON list of posts replied to ITT
    created_at TIMESTAMPTZ DEFAULT now(),
    updated_at TIMESTAMPTZ DEFAULT now(),
    CONSTRAINT "post_pkey" PRIMARY KEY ("id") NOT DEFERRABLE INITIALLY IMMEDIATE
);
CREATE INDEX posts_oleg_key ON mzbh.post (oleg_key);
DROP TRIGGER IF EXISTS set_updated_at ON "mzbh"."post";
CREATE TRIGGER set_updated_at
    BEFORE UPDATE ON "mzbh"."post"
    FOR EACH ROW EXECUTE PROCEDURE trigger_set_timestamp();

-- WEBM
CREATE TABLE mzbh.webm (
    id uuid DEFAULT public.uuid_generate_v4() NOT NULL,
    oleg_key TEXT UNIQUE,
    file_hash TEXT UNIQUE NOT NULL,
    filename TEXT,
    board TEXT,
    file_path TEXT,
    post_id uuid REFERENCES mzbh.post,
    created_at TIMESTAMPTZ DEFAULT now(),
    updated_at TIMESTAMPTZ DEFAULT now(),
    size INTEGER,
    CONSTRAINT "webm_pkey" PRIMARY KEY ("id") NOT DEFERRABLE INITIALLY IMMEDIATE
);
CREATE INDEX webm_oleg_key ON mzbh.webm (oleg_key);
CREATE INDEX webm_file_hash_idx ON webm (file_hash);
CREATE INDEX webm_filename_idx ON webm (filename);
DROP TRIGGER IF EXISTS set_updated_at ON "mzbh"."webm";
CREATE TRIGGER set_updated_at
    BEFORE UPDATE ON "mzbh"."webm"
    FOR EACH ROW EXECUTE PROCEDURE trigger_set_timestamp();

-- Aliases
CREATE TABLE webm_alias (
    id uuid DEFAULT public.uuid_generate_v4() NOT NULL,
    oleg_key TEXT,
    file_hash TEXT NOT NULL,
    filename TEXT,
    board TEXT,
    file_path TEXT,
    post_id uuid REFERENCES mzbh.post,
    webm_id uuid REFERENCES mzbh.webm,
    created_at TIMESTAMPTZ DEFAULT now(),
    updated_at TIMESTAMPTZ DEFAULT now(),
    CONSTRAINT "webm_alias_pkey" PRIMARY KEY ("id") NOT DEFERRABLE INITIALLY IMMEDIATE
);
CREATE INDEX webm_aliases_oleg_key ON mzbh.webm_alias (oleg_key);
CREATE INDEX webm_aliases_file_hash_idx ON mzbh.webm_alias (file_hash);
CREATE INDEX webm_aliases_filename_idx ON mzbh.webm_alias (filename);

DROP TRIGGER IF EXISTS set_updated_at ON "mzbh"."webm_alias";
CREATE TRIGGER set_updated_at
    BEFORE UPDATE ON "mzbh"."webm_alias"
    FOR EACH ROW EXECUTE PROCEDURE trigger_set_timestamp();

COMMIT;
