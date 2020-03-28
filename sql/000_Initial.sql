BEGIN;

CREATE TABLE threads (
    id SERIAL PRIMARY KEY,
    oleg_key TEXT UNIQUE,
    board TEXT,
    subject TEXT,
    created_at TIMESTAMPTZ DEFAULT now());
CREATE INDEX threads_oleg_key ON threads (oleg_key);

CREATE TABLE posts (
    id SERIAL PRIMARY KEY,
    oleg_key TEXT UNIQUE,
    fourchan_post_id BIGINT, -- The date of the post (unsignedix timestamp)
    fourchan_post_no BIGINT, -- The actual post ID on 4chan
    thread_id INTEGER REFERENCES threads,
    board TEXT,
    body_content TEXT,
    replied_to_keys JSONB, -- JSON list of posts replied to ITT
    created_at TIMESTAMPTZ DEFAULT now());
CREATE INDEX posts_oleg_key ON posts (oleg_key);

CREATE TABLE webms (
    id SERIAL PRIMARY KEY,
    oleg_key TEXT UNIQUE,
    file_hash TEXT UNIQUE NOT NULL,
    filename TEXT,
    board TEXT,
    file_path TEXT,
    post_id INTEGER REFERENCES posts,
    created_at TIMESTAMPTZ DEFAULT now(),
    size INTEGER);
CREATE INDEX webms_oleg_key ON webms (oleg_key);
CREATE INDEX webms_file_hash_idx ON webms (file_hash);
CREATE INDEX webms_filename_idx ON webms (filename);

CREATE TABLE webm_aliases (
    id SERIAL PRIMARY KEY,
    oleg_key TEXT,
    file_hash TEXT NOT NULL,
    filename TEXT,
    board TEXT,
    file_path TEXT,
    post_id INTEGER REFERENCES posts,
    webm_id INTEGER REFERENCES webms,
    created_at TIMESTAMPTZ DEFAULT now());
CREATE INDEX webm_aliases_oleg_key ON webms (oleg_key);
CREATE INDEX webm_aliases_file_hash_idx ON webms (file_hash);
CREATE INDEX webm_aliases_filename_idx ON webms (filename);

COMMIT;
