<table class = "<?php echo !empty($data['attributes']['class'][0]) ? $data['attributes']['class'][0] : '' ?> sticky-enabled" >
  <thead>
    <tr>
      <th><?php echo !empty($data['header']['placement']) ? $data['header']['placement'] : t('Placemenent') ?></th>
      <th><?php echo !empty($data['header']['copies']) ? $data['header']['copies'] : t('Copies') ?></th>
      <th><?php echo !empty($data['header']['at_home']) ? $data['header']['at_home'] : t('At home') ?></th> 
    </tr>
  </thead>
  <?php
  foreach ($data['rows'] as $element) {
    if (isset($element['data'])) {
      if (isset($element['class'])) {
        ?>
        <tr class="<?php echo !empty($element['data']['Class']) ? $element['data']['Class'] : '' ?>">
          <td><?php echo !empty($element['data']['Library']) ? $element['data']['Library'] : '' ?></td>
          <td><?php echo !empty($element['data']['Copies']) ? $element['data']['Copies'] : '' ?></td>
          <td><?php echo !empty($element['data']['Home']) ? $element['data']['Home'] : '' ?></td>       
        </tr>
        <?php
      }
      else {
        ?>
        <tr class="availability-holdings-total-row">
          <td><?php echo !empty($element['data']['Library']) ? $element['data']['Library'] : '' ?></td>
          <td><?php echo !empty($leement['data']['Copies']['data']) ? $element['data']['Copies']['data'] : '0' ?></td>
        </tr>
        <?php
      }
      ?>
      <?php
    }
    else {
      ?>
      <tr>
        <td><?php echo !empty($element['placement']) ? $element['placement'] : '' ?></td>
        <td><?php echo !empty($element['copies']) ? $element['copies'] : '' ?></td>
        <td><?php echo !empty($element['home']) ? $element['home'] : '' ?></td>       
      </tr>
      <?php
    }
    ?>
    <?php
  }
  ?>
</table>
