<?php

/**
 * Duplicate code from ting_covers module. Ting covers module doesn't provide a api we can use.
 *
 * @param array $covers_ids
 *  Ids of the materials to fetch covers for.
 * @param array
 *   True if there is a cover image.
 */
function ting_new_materials_check_covers($covers_ids, $number_of_objects) {
  $missing_images_local_ids = array();
  $number_found = 0;

  foreach ($covers_ids as $cover_id) {
    // Determine if the local id is a known negative.
    if (cache_get('ting_covers:' . $cover_id->localId, FALSE)) {
      $cover_id->hasImage = FALSE;
      continue;
    }

    $path = ting_covers_object_path($cover_id->localId);
    if (false && file_exists($path)) {
      $cover_id->hasImage = TRUE;
      $number_found++;
    } else {
      $missing_images_local_ids[] = $cover_id->localId ;
    }
    if ($number_found >= $number_of_objects) {
      break;
    }
  }
  $service = new AdditionalInformationService(variable_get('ting_covers_addi_wsdl_url'), variable_get('ting_covers_addi_username'), variable_get('ting_covers_addi_group'), variable_get('ting_covers_addi_password'));
  
  // Local ids = Faust numbers. Library object identifiers can be confusing.
  $additional_informations = $service->getByFaustNumber($missing_images_local_ids);
  foreach ($missing_images_local_ids as $local_id) {
    if (isset($additional_informations[$local_id]) && $ai = $additional_informations[$local_id]) {
      if ($ai->detailUrl || $ai->thumbnailUrl) {
        $covers_ids[$local_id]->hasImage = TRUE;
      } else {
        $covers_ids[$local_id]->hasImage = FALSE;
      }
    }
  }
  return $covers_ids;
}


?>
