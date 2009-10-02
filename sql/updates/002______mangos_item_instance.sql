UPDATE item_instance SET data = REPLACE(data,'  ',' ');
UPDATE item_instance SET data = CONCAT(TRIM(data),' ');

UPDATE item_instance SET data = CONCAT(SUBSTRING_INDEX(`data`, ' ', 6+0x2a), ' ')
WHERE length(SUBSTRING_INDEX(data, ' ', 6+0x30)) < length(data)
  and length(SUBSTRING_INDEX(data, ' ', 6+0x30+1)) >= length(data);

UPDATE item_instance SET data = CONCAT(
  SUBSTRING_INDEX(`data`, ' ', 6+0x2a), ' ',
  SUBSTRING_INDEX(`data`, ' ', -0x38-1),
  '0 0 ')
WHERE  length(SUBSTRING_INDEX(data, ' ', 110)) < length(data) and
 length(SUBSTRING_INDEX(data, ' ', 110+1)) >= length(data);


UPDATE item_instance SET data = REPLACE(data,'  ',' ');
UPDATE item_instance SET data = CONCAT(TRIM(data),' ');
