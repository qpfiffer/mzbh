SELECT webms.id, count(webm_aliases.id), 'http://tired/slurp/' || webms.board || '/' || webms.filename
FROM webms left join webm_aliases ON webms.id = webm_aliases.webm_id
GROUP BY webms.id
ORDER BY count(webm_aliases.id) DESC;
