ALTER TABLE `guild` CHANGE `EmblemStyle` `EmblemStyle` INT( 5 ) NOT NULL DEFAULT '0',
CHANGE `EmblemColor` `EmblemColor` INT( 5 ) NOT NULL DEFAULT '0',
CHANGE `BorderStyle` `BorderStyle` INT( 5 ) NOT NULL DEFAULT '0',
CHANGE `BorderColor` `BorderColor` INT( 5 ) NOT NULL DEFAULT '0',
CHANGE `BackgroundColor` `BackgroundColor` INT( 5 ) NOT NULL DEFAULT '0';

ALTER TABLE `item_template` CHANGE `Unknown1` `Map` INT( 10 ) NOT NULL DEFAULT '0';
ALTER TABLE `item_template` CHANGE `BagFamily` `BagFamily` INT( 10 ) NOT NULL DEFAULT '0' AFTER `Map`;

ALTER TABLE `group` CHANGE `leaderGuid` `leaderGuid` INT( 20 ) NOT NULL ,
CHANGE `looterGuid` `looterGuid` INT( 20 ) NOT NULL ;
ALTER TABLE `group_member` CHANGE `leaderGuid` `leaderGuid` INT( 20 ) NOT NULL ,
CHANGE `memberGuid` `memberGuid` INT( 20 ) NOT NULL ;

ALTER TABLE `creature_template` CHANGE `equipslot1` `equipslot1` INTEGER NOT NULL DEFAULT '0';
ALTER TABLE `creature_template` CHANGE `equipslot2` `equipslot2` INTEGER NOT NULL DEFAULT '0';
ALTER TABLE `creature_template` CHANGE `equipslot3` `equipslot3` INTEGER NOT NULL DEFAULT '0';

alter table `creature` change `state` `DeathState` tinyint(3) unsigned not null default '0';
